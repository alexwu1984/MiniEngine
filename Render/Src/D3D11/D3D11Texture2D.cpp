#include "D3D11/D3D11Texture2D.h"
#include "RHI/RHIDefinitions.h"
#include "RHIPrivate/D3D11RHIPrivate.h"
#include "D3D11/D3D11RHI.h"
#include "DirectXTex/DirectXTex.h"
#include "core/logger.h"
#include "tinygltf/stb_image.h"

namespace RenderCore
{
	struct D3D11Texture2DPrivate
	{
		D3D11DynamicRHI* D3D11RHI;
		win32::com_ptr<ID3D11Texture2D> Tex2D;
		win32::com_ptr<ID3D11Texture2D> DepthTex;
		win32::com_ptr<ID3D11ShaderResourceView> TexSRV;
		win32::com_ptr<ID3D11DepthStencilView> TexDSV;

		std::vector< win32::com_ptr <ID3D11RenderTargetView>> TexRTVS;

		core::vec2i Size;
		bool IsMultisampled = false;
	};

	/**
 * Creates a 2D texture optionally guarded by a structured exception handler.
 */
	bool SafeCreateTexture2D(ID3D11Device* Direct3DDevice, int32_t UEFormat, const D3D11_TEXTURE2D_DESC* TextureDesc, const D3D11_SUBRESOURCE_DATA* SubResourceData, ID3D11Texture2D** OutTexture2D)
	{
#if GUARDED_TEXTURE_CREATES
		bool bDriverCrash = true;
		__try
		{
#endif // #if GUARDED_TEXTURE_CREATES
			HRESULT hResult = Direct3DDevice->CreateTexture2D(TextureDesc, SubResourceData, OutTexture2D);
			VERIFYD3D11RESULT(hResult);
#if GUARDED_TEXTURE_CREATES
			bDriverCrash = false;
		}
		__finally
		{
			if (bDriverCrash)
			{
				LOG(core::log_err,
					TEXT("Driver crashed while creating texture: %ux%ux%u %s(0x%08x) with %u mips, PF_ %d"),
					TextureDesc->Width,
					TextureDesc->Height,
					TextureDesc->ArraySize,
					GetD3D11TextureFormatString(TextureDesc->Format),
					(uint32_t)TextureDesc->Format,
					TextureDesc->MipLevels,
					UEFormat
				);
				return false;
			}
			return true;
		}
#endif // #if GUARDED_TEXTURE_CREATES
	}

	D3D11Texture2D::D3D11Texture2D(D3D11DynamicRHI* D3D11RHI)
		:d_ptr(new D3D11Texture2DPrivate())
	{
		C_P(D3D11Texture2D);
		d->D3D11RHI = D3D11RHI;
	}

	D3D11Texture2D::~D3D11Texture2D()
	{

	}

	bool D3D11Texture2D::CreateD3D11Texture2D(EPixelFormat Format, int32_t Flags, int32_t SizeX, int32_t SizeY, int32_t SizeZ, void* InBuffer /*= nullptr*/, int32_t RowBytes /*= 0*/)
	{
		C_P(D3D11Texture2D);
		d->Size.cx = SizeX;
		d->Size.cy = SizeY;
		return CreateD3D11Texture2D(Format,Flags,SizeX,SizeY,SizeZ,false,1, InBuffer,RowBytes);
	}

	bool D3D11Texture2D::CreateD3D11Texture2D(EPixelFormat Format, int32_t Flags, int32_t SizeX, int32_t SizeY, int32_t SizeZ, 
		bool bCubeTexture, uint32_t NumMips, void* InBuffer, int RowBytes)
	{
		C_P(D3D11Texture2D);
		const bool bSRGB = (Flags & TexCreate_SRGB) != 0;

		const DXGI_FORMAT PlatformResourceFormat = GetPlatformTextureResourceFormat((DXGI_FORMAT)GPixelFormats[Format].PlatformFormat, Flags);

		uint32_t CPUAccessFlags = 0;
		D3D11_USAGE TextureUsage = D3D11_USAGE_DEFAULT;
		bool bCreateShaderResource = true;

		auto Device = d->D3D11RHI->GetDevice();

		uint32_t ActualMSAACount = 1;
		uint32_t ActualMSAAQuality = 0;
		if (Flags & TexCreate_MSAA)
		{
			ActualMSAAQuality = GetMaxMSAAQuality(Device, PlatformResourceFormat, ActualMSAACount);
			// 0xffffffff means not supported
			if (ActualMSAAQuality == 0xffffffff || (Flags & TexCreate_Shared) != 0)
			{
				// no MSAA
				ActualMSAACount = 1;
				ActualMSAAQuality = 0;
			}
		}

		d->IsMultisampled = ActualMSAACount > 1;

		if (Flags & TexCreate_CPUReadback)
		{
			Assert(!(Flags & TexCreate_RenderTargetable));
			Assert(!(Flags & TexCreate_DepthStencilTargetable));
			Assert(!(Flags & TexCreate_ShaderResource));

			CPUAccessFlags = D3D11_CPU_ACCESS_READ;
			TextureUsage = D3D11_USAGE_STAGING;
			bCreateShaderResource = false;
		}

		if (Flags & TexCreate_CPUWritable)
		{
			CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
			TextureUsage = D3D11_USAGE_STAGING;
			bCreateShaderResource = false;
		}

		// Describe the texture.
		D3D11_TEXTURE2D_DESC TextureDesc;
		ZeroMemory(&TextureDesc, sizeof(D3D11_TEXTURE2D_DESC));
		TextureDesc.Width = SizeX;
		TextureDesc.Height = SizeY;
		TextureDesc.MipLevels = NumMips;
		TextureDesc.ArraySize = SizeZ;
		TextureDesc.Format = PlatformResourceFormat;
		TextureDesc.SampleDesc.Count = ActualMSAACount;
		TextureDesc.SampleDesc.Quality = ActualMSAAQuality;
		TextureDesc.Usage = TextureUsage;
		TextureDesc.BindFlags = bCreateShaderResource ? D3D11_BIND_SHADER_RESOURCE : 0;
		TextureDesc.CPUAccessFlags = CPUAccessFlags;
		TextureDesc.MiscFlags = bCubeTexture ? D3D11_RESOURCE_MISC_TEXTURECUBE : 0;

		if (Flags & TexCreate_DisableSRVCreation)
		{
			bCreateShaderResource = false;
		}

		if (Flags & TexCreate_Shared)
		{
			TextureDesc.MiscFlags |= D3D11_RESOURCE_MISC_SHARED;
		}

		if (Flags & TexCreate_GenerateMipCapable)
		{
			// Set the flag that allows us to call GenerateMips on this texture later
			TextureDesc.MiscFlags |= D3D11_RESOURCE_MISC_GENERATE_MIPS;
			TextureDesc.MipLevels = 0;
		}

		// Set up the texture bind flags.
		bool bCreateRTV = false;
		bool bCreateDSV = false;

		if (Flags & TexCreate_RenderTargetable || d->IsMultisampled || Flags & TexCreate_GenerateMipCapable)
		{
			Assert(!(Flags & TexCreate_DepthStencilTargetable));
			Assert(!(Flags & TexCreate_ResolveTargetable));
			TextureDesc.BindFlags |= D3D11_BIND_RENDER_TARGET;
			bCreateRTV = true;
		}
		else if (Flags & TexCreate_DepthStencilTargetable)
		{
			Assert(!(Flags & TexCreate_RenderTargetable));
			Assert(!(Flags & TexCreate_ResolveTargetable));
			TextureDesc.BindFlags |= D3D11_BIND_DEPTH_STENCIL;
			bCreateDSV = true;
		}
		else if (Flags & TexCreate_ResolveTargetable)
		{
			Assert(!(Flags & TexCreate_RenderTargetable));
			Assert(!(Flags & TexCreate_DepthStencilTargetable));
			if (Format == PF_DepthStencil || Format == PF_ShadowDepth || Format == PF_D24)
			{
				TextureDesc.BindFlags |= D3D11_BIND_DEPTH_STENCIL;
				bCreateDSV = true;
			}
			else
			{
				TextureDesc.BindFlags |= D3D11_BIND_RENDER_TARGET;
				bCreateRTV = true;
			}
		}

		if (Flags & TexCreate_UAV)
		{
			TextureDesc.BindFlags |= D3D11_BIND_UNORDERED_ACCESS;
		}

		if (bCreateDSV && !(Flags & TexCreate_ShaderResource))
		{
			TextureDesc.BindFlags &= ~D3D11_BIND_SHADER_RESOURCE;
			bCreateShaderResource = false;
		}


		auto DeviceContext = d->D3D11RHI->GetDeviceContext();
		HRESULT hr = S_OK;
		if (Flags & TexCreate_GenerateMipCapable)
		{
			if (!SafeCreateTexture2D(Device, Format, &TextureDesc, nullptr, d->Tex2D.get_init_ref()))
			{
				return false;
			}

			if (InBuffer)
			{
				DeviceContext->UpdateSubresource(d->Tex2D.get(), 0, nullptr, InBuffer, RowBytes, 0);
			}
		}
		else
		{
			D3D11_SUBRESOURCE_DATA SubRes{};
			SubRes.pSysMem = InBuffer;
			SubRes.SysMemPitch = RowBytes;
			SubRes.SysMemSlicePitch = SizeY * RowBytes;

			if (!SafeCreateTexture2D(Device, Format, &TextureDesc, InBuffer ? &SubRes : nullptr, d->Tex2D.get_init_ref()))
			{
				return false;
			}
		}

		if (bCreateShaderResource)
		{
			D3D11_SHADER_RESOURCE_VIEW_DESC SRVDesc;
			memset(&SRVDesc, 0, sizeof(SRVDesc));
			SRVDesc.Format = TextureDesc.Format;

			if (bCubeTexture)
			{
				SRVDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURECUBE;
				SRVDesc.TextureCube.MostDetailedMip = 0;
				SRVDesc.TextureCube.MipLevels = NumMips;
			}
			else
			{
				if (d->IsMultisampled)
				{
					SRVDesc.ViewDimension = D3D_SRV_DIMENSION_TEXTURE2DMS;
				}
				else
				{
					SRVDesc.ViewDimension = D3D_SRV_DIMENSION_TEXTURE2D;
					SRVDesc.Texture2D.MostDetailedMip = 0;
					SRVDesc.Texture2D.MipLevels = NumMips;
				}
			}


			hr = Device->CreateShaderResourceView(d->Tex2D.get(), &SRVDesc, d->TexSRV.get_init_ref());
			if (FAILED(hr))
			{
				return false;
			}

			if ((Flags & TexCreate_GenerateMipCapable) && d->TexSRV)
			{
				DeviceContext->GenerateMips(d->TexSRV.get());
			}

		}

		if (bCreateRTV)
		{
			if (bCubeTexture)
			{
				for (uint32_t MipIndex = 0; MipIndex < NumMips; MipIndex++)
				{
					for (uint32_t SliceIndex = 0; SliceIndex < TextureDesc.ArraySize; SliceIndex++)
					{
						D3D11_RENDER_TARGET_VIEW_DESC RTVDesc;
						memset(&RTVDesc, 0, sizeof(RTVDesc));

						RTVDesc.Format = TextureDesc.Format;

						if (d->IsMultisampled)
						{
							RTVDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2DMSARRAY;
							RTVDesc.Texture2DMSArray.FirstArraySlice = SliceIndex;
							RTVDesc.Texture2DMSArray.ArraySize = 1;
						}
						else
						{
							RTVDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2DARRAY;
							RTVDesc.Texture2DArray.FirstArraySlice = SliceIndex;
							RTVDesc.Texture2DArray.ArraySize = 1;
							RTVDesc.Texture2DArray.MipSlice = MipIndex;
						}

						win32::com_ptr<ID3D11RenderTargetView> TexRTV;
						hr = Device->CreateRenderTargetView(d->Tex2D.get(), &RTVDesc, TexRTV.get_init_ref());
						d->TexRTVS.emplace_back(TexRTV);
					}
				}
			}
			else
			{
				for (uint32_t MipIndex = 0; MipIndex < NumMips; MipIndex++)
				{
					D3D11_RENDER_TARGET_VIEW_DESC RTVDesc;
					memset(&RTVDesc, 0, sizeof(RTVDesc));
					RTVDesc.Format = TextureDesc.Format;
					if (d->IsMultisampled)
					{
						RTVDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2DMS;
					}
					else
					{
						RTVDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
					}
					RTVDesc.Texture2D.MipSlice = MipIndex;
					win32::com_ptr<ID3D11RenderTargetView> TexRTV;
					hr = Device->CreateRenderTargetView(d->Tex2D.get(), &RTVDesc, TexRTV.get_init_ref());
					if (FAILED(hr))
					{
						return false;
					}
					d->TexRTVS.emplace_back(TexRTV);
				}
			}
		}

		if (bCreateDSV)
		{
			D3D11_TEXTURE2D_DESC DepthStencilDesc;
			ZeroMemory(&DepthStencilDesc, sizeof(DepthStencilDesc));
			DepthStencilDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
			DepthStencilDesc.Width = SizeX;
			DepthStencilDesc.Height = SizeY;
			DepthStencilDesc.MipLevels = 1;
			DepthStencilDesc.ArraySize = 1;
			DepthStencilDesc.Usage = D3D11_USAGE_DEFAULT;
			DepthStencilDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL;
			DepthStencilDesc.CPUAccessFlags = 0;
			DepthStencilDesc.MiscFlags = 0;
			DepthStencilDesc.SampleDesc.Count = 1;
			DepthStencilDesc.SampleDesc.Quality = 0;
			hr = Device->CreateTexture2D(&DepthStencilDesc, 0, d->DepthTex.get_init_ref());
			if (FAILED(hr))
			{
				return false;
			}
			D3D11_DEPTH_STENCIL_VIEW_DESC DSVDesc;
			memset(&DSVDesc, 0, sizeof(DSVDesc));
			DSVDesc.Format = TextureDesc.Format;
			if (d->IsMultisampled)
			{
				DSVDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2DMS;
			}
			else
			{
				DSVDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
			}

			DSVDesc.Texture2D.MipSlice = 0;
			hr = Device->CreateDepthStencilView(d->DepthTex.get(), &DSVDesc, d->TexDSV.get_init_ref());
			if (FAILED(hr))
			{
				return false;
			}
		}

		return true;
	}

	bool D3D11Texture2D::CreateFromFile(const std::wstring& FileName)
	{
		std::string Utf8FileName = core::ucs2_u8(FileName);
		int32_t ImageChannel = 0;
		int32_t SizeX = 0;
		int32_t SizeY = 0;
		std::shared_ptr<uint8_t> ImageBuffer(stbi_load(Utf8FileName.c_str(), &SizeX, &SizeY, &ImageChannel, 4), [](uint8_t* p) {stbi_image_free(p); });
		if (ImageBuffer)
		{
			return CreateD3D11Texture2D(PF_B8G8R8A8, TexCreate_ShaderResource, SizeX, SizeY, 1,ImageBuffer.get(), 4 * SizeX * sizeof(uint8_t));
		}
		return false;
	}

	bool D3D11Texture2D::CreateHDRFromFile(const std::wstring& FileName)
	{
		std::string Utf8FileName = core::ucs2_u8(FileName);
		int32_t ImageChannel = 0;
		int32_t SizeX = 0;
		int32_t SizeY = 0;
		std::shared_ptr<uint8_t> ImageBuffer(stbi_load(Utf8FileName.c_str(), &SizeX, &SizeY, &ImageChannel, 4), [](uint8_t* p) {stbi_image_free(p); });
		if (ImageBuffer)
		{
			return CreateD3D11Texture2D(PF_FloatRGBA, TexCreate_ShaderResource, SizeX, SizeY, 1,ImageBuffer.get(), 4 * SizeX * sizeof(float));
		}
		return false;
	}

	bool D3D11Texture2D::IsMultisampled() const
	{
		C_P(D3D11Texture2D);
		return d->IsMultisampled;
	}

	core::vec2i D3D11Texture2D::GetSize() const
	{
		C_P(D3D11Texture2D);
		return d->Size;
	}

	ID3D11Texture2D* D3D11Texture2D::GetNativeTex() const
	{
		C_P(D3D11Texture2D);
		return d->Tex2D.get();
	}

	ID3D11RenderTargetView* D3D11Texture2D::GetRTV() const
	{
		C_P(D3D11Texture2D);
		if (d->TexRTVS.empty())
		{
			return nullptr;
		}
		return d->TexRTVS[0].get();
	}

	std::vector<win32::com_ptr<ID3D11RenderTargetView>> D3D11Texture2D::GetRTVS() const
	{
		C_P(D3D11Texture2D);
		return d->TexRTVS;
	}

	ID3D11ShaderResourceView* D3D11Texture2D::GetSRV() const
	{
		C_P(D3D11Texture2D);
		return d->TexSRV.get();
	}

	ID3D11DepthStencilView* D3D11Texture2D::GetDSV() const
	{
		C_P(D3D11Texture2D);
		return d->TexDSV.get();
	}

}