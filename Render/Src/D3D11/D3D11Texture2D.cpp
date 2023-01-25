#include "D3D11/D3D11Texture2D.h"
#include "RHI/RHIDefinitions.h"
#include "RHIPrivate/D3D11RHIPrivate.h"
#include "D3D11/D3D11RHI.h"
#include "DirectXTex/DirectXTex.h"
#include "core/logger.h"
#include "tinygltf/stb_image.h"

namespace RenderCore
{
	struct D3D11Texture2DP
	{
		D3D11DynamicRHI* D3D11RHI;
		win32::com_ptr<ID3D11Texture2D> Tex2D;
		win32::com_ptr<ID3D11ShaderResourceView> TexSRV;
		win32::com_ptr<ID3D11RenderTargetView> TexRTV;
		win32::com_ptr<ID3D11DepthStencilView> TexDSV;
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
		:Impl(std::make_shared<D3D11Texture2DP>())
	{
		Impl->D3D11RHI = D3D11RHI;
	}

	D3D11Texture2D::~D3D11Texture2D()
	{

	}

	bool D3D11Texture2D::CreateWithData(EPixelFormat Format, int32_t Flags, int32_t SizeX, int32_t SizeY, void* InBuffer /*= nullptr*/, int32_t RowBytes /*= 0*/)
	{
		Impl->Size.cx = SizeX;
		Impl->Size.cy = SizeY;
		const bool bSRGB = (Flags & TexCreate_SRGB) != 0;

		const DXGI_FORMAT PlatformResourceFormat = GetPlatformTextureResourceFormat((DXGI_FORMAT)GPixelFormats[Format].PlatformFormat, Flags);

		uint32_t CPUAccessFlags = 0;
		D3D11_USAGE TextureUsage = D3D11_USAGE_DEFAULT;
		bool bCreateShaderResource = true;

		auto Device = Impl->D3D11RHI->GetDevice();

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

		Impl->IsMultisampled = ActualMSAACount > 1;

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
		TextureDesc.MipLevels = 1;
		TextureDesc.ArraySize = 1;
		TextureDesc.Format = PlatformResourceFormat;
		TextureDesc.SampleDesc.Count = ActualMSAACount;
		TextureDesc.SampleDesc.Quality = ActualMSAAQuality;
		TextureDesc.Usage = TextureUsage;
		TextureDesc.BindFlags = bCreateShaderResource ? D3D11_BIND_SHADER_RESOURCE : 0;
		TextureDesc.CPUAccessFlags = CPUAccessFlags;
		TextureDesc.MiscFlags =  0;

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

		if (Flags & TexCreate_RenderTargetable || Impl->IsMultisampled || Flags & TexCreate_GenerateMipCapable)
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

		
		auto DeviceContext = Impl->D3D11RHI->GetDeviceContext();
		HRESULT hr = S_OK;
		if (Flags & TexCreate_GenerateMipCapable)
		{
			if (!SafeCreateTexture2D(Device, Format, &TextureDesc, nullptr, Impl->Tex2D.get_init_ref()))
			{
				return false;
			}

			if (InBuffer)
			{
				DeviceContext->UpdateSubresource(Impl->Tex2D.get(), 0, nullptr, InBuffer, RowBytes, 0);
			}
		}
		else
		{
			D3D11_SUBRESOURCE_DATA SubRes{};
			SubRes.pSysMem = InBuffer;
			SubRes.SysMemPitch = RowBytes;
			SubRes.SysMemSlicePitch = SizeY * RowBytes;

			if (!SafeCreateTexture2D(Device, Format, &TextureDesc, &SubRes, Impl->Tex2D.get_init_ref()))
			{
				return false;
			}
		}

		if (bCreateShaderResource)
		{
			D3D11_SHADER_RESOURCE_VIEW_DESC SRVDesc;
			memset(&SRVDesc, 0, sizeof(SRVDesc));
			SRVDesc.Format = TextureDesc.Format;
			if (Impl->IsMultisampled)
			{
				SRVDesc.ViewDimension = D3D_SRV_DIMENSION_TEXTURE2DMS;
			}
			else
			{
				SRVDesc.ViewDimension = D3D_SRV_DIMENSION_TEXTURE2D;
			}
			//Set to -1 to indicate all the mipmap levels from MostDetailedMip on down to least detailed
			SRVDesc.Texture2D.MipLevels = -1;
			SRVDesc.Texture2D.MostDetailedMip = 0;
			
			hr = Device->CreateShaderResourceView(Impl->Tex2D.get(), &SRVDesc, Impl->TexSRV.get_init_ref());
			if (FAILED(hr))
			{
				return false;
			}
			
			if ((Flags & TexCreate_GenerateMipCapable) && Impl->TexSRV)
			{
				DeviceContext->GenerateMips(Impl->TexSRV.get());
			}

		}

		if (bCreateRTV)
		{
			D3D11_RENDER_TARGET_VIEW_DESC RTVDesc;
			memset(&RTVDesc, 0, sizeof(RTVDesc));
			RTVDesc.Format = TextureDesc.Format;
			if (Impl->IsMultisampled)
			{
				RTVDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2DMS;
			}
			else
			{
				RTVDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
			}
			RTVDesc.Texture2D.MipSlice = 0;
			hr = Device->CreateRenderTargetView(Impl->Tex2D.get(), &RTVDesc, Impl->TexRTV.get_init_ref());
			if (FAILED(hr))
			{
				return false;
			}
		}

		if (bCreateDSV)
		{
			D3D11_DEPTH_STENCIL_VIEW_DESC DSVDesc;
			memset(&DSVDesc, 0, sizeof(DSVDesc));
			DSVDesc.Format = TextureDesc.Format;
			if (Impl->IsMultisampled)
			{
				DSVDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2DMS;
			}
			else
			{
				DSVDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
			}

			DSVDesc.Texture2D.MipSlice = 0;
			hr = Device->CreateDepthStencilView(Impl->Tex2D.get(), &DSVDesc, Impl->TexDSV.get_init_ref());
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
			return CreateWithData(PF_B8G8R8A8, TexCreate_ShaderResource, SizeX, SizeY, ImageBuffer.get(), 4 * SizeX * sizeof(uint8_t));
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
			return CreateWithData(PF_FloatRGBA, TexCreate_ShaderResource, SizeX, SizeY, ImageBuffer.get(), 4 * SizeX * sizeof(float));
		}
		return false;
	}

	bool D3D11Texture2D::IsMultisampled() const
	{
		return Impl->IsMultisampled;
	}

	core::vec2i D3D11Texture2D::GetSize() const
	{
		return Impl->Size;
	}

	ID3D11Texture2D* D3D11Texture2D::GetNativeTex() const
	{
		return Impl->Tex2D.get();
	}

	ID3D11RenderTargetView* D3D11Texture2D::GetRTV() const
	{
		return Impl->TexRTV.get();
	}

	ID3D11ShaderResourceView* D3D11Texture2D::GetSRV() const
	{
		return Impl->TexSRV.get();
	}

	ID3D11DepthStencilView* D3D11Texture2D::GetDSV() const
	{
		return Impl->TexDSV.get();
	}

}