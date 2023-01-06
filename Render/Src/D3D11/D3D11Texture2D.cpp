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
	};

	/**
 * Creates a 2D texture optionally guarded by a structured exception handler.
 */
	void SafeCreateTexture2D(ID3D11Device* Direct3DDevice, int32_t UEFormat, const D3D11_TEXTURE2D_DESC* TextureDesc, const D3D11_SUBRESOURCE_DATA* SubResourceData, ID3D11Texture2D** OutTexture2D)
	{
#if GUARDED_TEXTURE_CREATES
		bool bDriverCrash = true;
		__try
		{
#endif // #if GUARDED_TEXTURE_CREATES
			VERIFYD3D11RESULT(
				Direct3DDevice->CreateTexture2D(TextureDesc, SubResourceData, OutTexture2D)
			);
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
			}
		}
#endif // #if GUARDED_TEXTURE_CREATES
	}

	D3D11Texture2D::D3D11Texture2D(D3D11DynamicRHI* D3D11RHI)
		:Data(std::make_shared<D3D11Texture2DP>())
	{
		Data->D3D11RHI = D3D11RHI;
	}

	D3D11Texture2D::~D3D11Texture2D()
	{

	}

	bool D3D11Texture2D::InitTexture(uint32_t Format, uint32_t Flags, int32_t SizeX, int32_t SizeY, void* InBuffer /*= nullptr*/, int32_t RowBytes /*= 0*/)
	{

		const bool bSRGB = (Flags & TexCreate_SRGB) != 0;

		const DXGI_FORMAT PlatformResourceFormat = GetPlatformTextureResourceFormat((DXGI_FORMAT)GPixelFormats[Format].PlatformFormat, Flags);
		const DXGI_FORMAT PlatformShaderResourceFormat = FindShaderResourceDXGIFormat(PlatformResourceFormat, bSRGB);
		const DXGI_FORMAT PlatformRenderTargetFormat = FindShaderResourceDXGIFormat(PlatformResourceFormat, bSRGB);

		uint32_t CPUAccessFlags = 0;
		D3D11_USAGE TextureUsage = D3D11_USAGE_DEFAULT;
		bool bCreateShaderResource = true;

		uint32_t ActualMSAACount = 4;

		uint32_t ActualMSAAQuality = GetMaxMSAAQuality(ActualMSAACount);

		// 0xffffffff means not supported
		if (ActualMSAAQuality == 0xffffffff || (Flags & TexCreate_Shared) != 0)
		{
			// no MSAA
			ActualMSAACount = 1;
			ActualMSAAQuality = 0;
		}

		const bool bIsMultisampled = ActualMSAACount > 1;

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

		if (Flags & TexCreate_RenderTargetable || bIsMultisampled || Flags & TexCreate_GenerateMipCapable)
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

		auto Device = Data->D3D11RHI->GetDevice();
		auto DeviceContext = Data->D3D11RHI->GetDeviceContext();
		HRESULT hr = S_OK;
		if (Flags & TexCreate_GenerateMipCapable)
		{
			hr = Device->CreateTexture2D(&TextureDesc, nullptr, Data->Tex2D.get_init_ref());
			if (FAILED(hr))
			{
				return false;
			}
			if (InBuffer != NULL)
			{
				DeviceContext->UpdateSubresource(Data->Tex2D.get(), 0, nullptr, InBuffer, RowBytes, 0);
			}
		}
		else
		{
			D3D11_SUBRESOURCE_DATA initData{};
			initData.pSysMem = InBuffer;
			initData.SysMemPitch = RowBytes;
			initData.SysMemSlicePitch = SizeY * RowBytes;

			hr = Device->CreateTexture2D(&TextureDesc, &initData,Data->Tex2D.get_init_ref());
			if (FAILED(hr))
			{
				return false;
			}
		}

		if (bCreateShaderResource)
		{
			D3D11_SHADER_RESOURCE_VIEW_DESC SRVDesc;
			memset(&SRVDesc, 0, sizeof(SRVDesc));
			SRVDesc.Format = TextureDesc.Format;
			if (bIsMultisampled)
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
			
			hr = Device->CreateShaderResourceView(Data->Tex2D.get(), &SRVDesc, Data->TexSRV.get_init_ref());
			if (FAILED(hr))
			{
				return false;
			}
			
			if ((Flags & TexCreate_GenerateMipCapable) && Data->TexSRV)
			{
				DeviceContext->GenerateMips(Data->TexSRV.get());
			}

		}

		if (bCreateRTV)
		{
			D3D11_RENDER_TARGET_VIEW_DESC RTVDesc;
			memset(&RTVDesc, 0, sizeof(RTVDesc));
			RTVDesc.Format = TextureDesc.Format;
			if (bIsMultisampled)
			{
				RTVDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2DMS;
			}
			else
			{
				RTVDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
			}
			RTVDesc.Texture2D.MipSlice = 0;
			hr = Device->CreateRenderTargetView(Data->Tex2D.get(), &RTVDesc, Data->TexRTV.get_init_ref());
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
			if (bIsMultisampled)
			{
				DSVDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2DMS;
			}
			else
			{
				DSVDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
			}

			DSVDesc.Texture2D.MipSlice = 0;
			hr = Device->CreateDepthStencilView(Data->Tex2D.get(), &DSVDesc, Data->TexDSV.get_init_ref());
			if (FAILED(hr))
			{
				return false;
			}
		}

		return true;
	}

}