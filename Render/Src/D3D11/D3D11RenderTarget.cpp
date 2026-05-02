#include "D3D11/D3D11RenderTarget.h"
#include "RHIPrivate/D3D11RHIPrivate.h"
#include "D3D11/D3D11RHI.h"
#include "D3D11/D3D11Texture2D.h"

namespace RenderCore
{
	struct D3D11RenderTargetPrivate
	{
		D3D11DynamicRHI* D3D11RHI = nullptr;
		std::shared_ptr< D3D11Texture2D> Tex2D;
		std::shared_ptr< D3D11Texture2D> DepthTex;
		core::vec2i Size;

		D3D11RenderTargetPrivate(D3D11DynamicRHI* RHI) :
			D3D11RHI(RHI)
		{

		}
	};

	D3D11RenderTarget::D3D11RenderTarget(D3D11DynamicRHI* D3D11RHI)
		:d_ptr(new D3D11RenderTargetPrivate(D3D11RHI))

	{
		
	}

	D3D11RenderTarget::~D3D11RenderTarget()
	{
		delete d_ptr;
	}


	bool D3D11RenderTarget::Create(EPixelFormat Format, int32_t SizeX, int32_t SizeY, uint32_t NumMips, bool IsMultiSampled, bool CreateDepth)
	{
		C_P(D3D11RenderTarget);
		d->Size = core::vec2i(SizeX, SizeY);
		d->Tex2D = std::make_shared<D3D11Texture2D>(d->D3D11RHI);
		int32_t Flags = ETextureCreateFlags::TexCreate_RenderTargetable | ETextureCreateFlags::TexCreate_ShaderResource;
		//if (NumMips > 1)
		//{
		//	Flags |= TexCreate_GenerateMipCapable;
		//}
		if (IsMultiSampled)
		{
			Flags |= ETextureCreateFlags::TexCreate_MSAA;
		}

		if (!d->Tex2D->CreateTexture2D(Format, Flags,SizeX,SizeY,1, NumMips))
		{
			return false;
		}
		if (CreateDepth)
		{
			d->DepthTex = std::make_shared<D3D11Texture2D>(d->D3D11RHI);
			Flags = ETextureCreateFlags::TexCreate_DepthStencilTargetable;
			if (IsMultiSampled)
			{
				Flags |= ETextureCreateFlags::TexCreate_MSAA;
			}

			return d->DepthTex->CreateTexture2D(EPixelFormat::PF_DepthStencil, Flags, SizeX, SizeY);
		}
		return true;
	}

	core::vec2i D3D11RenderTarget::GetSize() const
	{
		C_P(D3D11RenderTarget);
		return d->Size;
	}

	void D3D11RenderTarget::Bind()
	{
		C_P(D3D11RenderTarget);
		d->D3D11RHI->GetDefaultCommandContext()->SetRenderTarget(d->Tex2D,d->DepthTex);
	}

	void D3D11RenderTarget::UnBind()
	{
		C_P(D3D11RenderTarget);
		d->D3D11RHI->GetDefaultCommandContext()->SetRenderTarget(nullptr, nullptr);
	}

	ID3D11Texture2D* D3D11RenderTarget::GetNativeTex() const
	{
		C_P(D3D11RenderTarget);
		if (!d->Tex2D)
		{
			return nullptr;
		}
		return d->Tex2D->GetNativeTex();
	}

	ID3D11RenderTargetView* D3D11RenderTarget::GetRTV() const
	{
		C_P(const D3D11RenderTarget);
		if (!d->Tex2D)
		{
			return nullptr;
		}
		return d->Tex2D->GetRTV();
	}

	ID3D11ShaderResourceView* D3D11RenderTarget::GetSRV() const
	{
		C_P(const D3D11RenderTarget);
		if (!d->Tex2D)
		{
			return nullptr;
		}
		return d->Tex2D->GetSRV();
	}

	ID3D11DepthStencilView* D3D11RenderTarget::GetDSV() const
	{
		C_P(const D3D11RenderTarget);
		if (!d->DepthTex)
		{
			return nullptr;
		}
		return d->DepthTex->GetDSV();
	}

	std::map < uint32_t, std::vector< win32::com_ptr <ID3D11RenderTargetView>>> D3D11RenderTarget::GetRTVS() const
	{
		C_P(const D3D11RenderTarget);
		return d->Tex2D->GetRTVS();
	}

	std::map < uint32_t, std::vector< win32::com_ptr <ID3D11RenderTargetView>>>& D3D11RenderTarget::GetRTVS()
	{
		C_P(const D3D11RenderTarget);
		return d->Tex2D->GetRTVS();
	}

	std::shared_ptr< RenderCore::RHITexture2D> D3D11RenderTarget::GetTex() const
	{
		C_P(const D3D11RenderTarget);
		return d->Tex2D;
	}

}