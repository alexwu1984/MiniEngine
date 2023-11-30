#include "D3D11/D3D11RenderTarget.h"
#include "RHIPrivate/D3D11RHIPrivate.h"
#include "D3D11/D3D11RHI.h"
#include "D3D11/D3D11ReourceTraits.h"

namespace RenderCore
{
	struct D3D11RenderTargetP
	{
		D3D11DynamicRHI* D3D11RHI = nullptr;
		std::shared_ptr< D3D11Texture2D> Tex2D;
		std::shared_ptr< D3D11Texture2D> DepthTex;
	};

	D3D11RenderTarget::D3D11RenderTarget(D3D11DynamicRHI* D3D11RHI)
		:Data(std::make_shared<D3D11RenderTargetP>())

	{
		Data->D3D11RHI = D3D11RHI;
	}

	D3D11RenderTarget::~D3D11RenderTarget()
	{

	}


	bool D3D11RenderTarget::Create(EPixelFormat Format, int32_t SizeX, int32_t SizeY, bool IsMultiSampled, bool CreateDepth)
	{
		Data->Tex2D = std::make_shared<D3D11Texture2D>(Data->D3D11RHI);
		int32_t Flags = ETextureCreateFlags::TexCreate_RenderTargetable;
		if (IsMultiSampled)
		{
			Flags |= ETextureCreateFlags::TexCreate_MSAA;
		}

		if (!Data->Tex2D->CreateD3D11Texture2D(Format, Flags,SizeX,SizeY))
		{
			return false;
		}
		if (CreateDepth)
		{
			Data->DepthTex = std::make_shared<D3D11Texture2D>(Data->D3D11RHI);
			Flags = ETextureCreateFlags::TexCreate_DepthStencilTargetable;
			if (IsMultiSampled)
			{
				Flags |= ETextureCreateFlags::TexCreate_MSAA;
			}

			return Data->DepthTex->CreateD3D11Texture2D(EPixelFormat::PF_DepthStencil, Flags, SizeX, SizeY);
		}
		return true;
	}

	void D3D11RenderTarget::Bind()
	{
		Data->D3D11RHI->GetDefaultCommandContext()->SetRenderTarget(Data->Tex2D,Data->DepthTex);
	}

	void D3D11RenderTarget::UnBind()
	{
		Data->D3D11RHI->GetDefaultCommandContext()->SetRenderTarget(nullptr, nullptr);
	}

	ID3D11Texture2D* D3D11RenderTarget::GetNativeTex() const
	{
		if (!Data->Tex2D)
		{
			return nullptr;
		}
		return Data->Tex2D->GetNativeTex();
	}

	ID3D11RenderTargetView* D3D11RenderTarget::GetRTV() const
	{
		if (!Data->Tex2D)
		{
			return nullptr;
		}
		return Data->Tex2D->GetRTV();
	}

	ID3D11ShaderResourceView* D3D11RenderTarget::GetSRV() const
	{
		if (!Data->Tex2D)
		{
			return nullptr;
		}
		return Data->Tex2D->GetSRV();
	}

	ID3D11DepthStencilView* D3D11RenderTarget::GetDSV() const
	{
		if (!Data->DepthTex)
		{
			return nullptr;
		}
		return Data->DepthTex->GetDSV();
	}

}