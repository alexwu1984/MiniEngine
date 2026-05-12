#include "D3D11/D3D11TextureCube.h"
#include "D3D11/D3D11Texture2D.h"
#include "RHI/RHIDefinitions.h"

namespace RenderCore
{
	struct D3D11TextureCubePrivate
	{
		D3D11DynamicRHI* D3D11RHI = nullptr;
		std::shared_ptr<D3D11Texture2D> Tex2D;
		std::shared_ptr<D3D11Texture2D> DepthTex;
	};

	D3D11TextureCube::D3D11TextureCube(D3D11DynamicRHI* D3D11RHI)
		:d_ptr(new D3D11TextureCubePrivate())
	{
		C_P(D3D11TextureCube);
		d->D3D11RHI = D3D11RHI;
		d->Tex2D = std::make_shared<D3D11Texture2D>(D3D11RHI);
		d->DepthTex = std::make_shared<D3D11Texture2D>(D3D11RHI);
	}

	D3D11TextureCube::~D3D11TextureCube()
	{
		delete d_ptr;
	}

	bool D3D11TextureCube::CreateTextureCube(EPixelFormat Format, int32_t SizeX, int32_t SizeY, uint32_t NumMips, bool CreateDepth)
	{
		C_P(D3D11TextureCube);
		if (Format == EPixelFormat::PF_ShadowDepth)
		{
			const int32_t flags = TexCreate_DepthStencilTargetable | TexCreate_ShaderResource;
			if (!d->Tex2D->CreateTexture2D(Format, flags, SizeX, SizeY, 6, true, NumMips, nullptr, 0))
				return false;
			d->DepthTex = d->Tex2D;
			return true;
		}
		bool Ret = d->Tex2D->CreateTexture2D(Format, TexCreate_ShaderResource | TexCreate_RenderTargetable | TexCreate_GenerateMipCapable, SizeX, SizeY, 6, true, NumMips, nullptr, 0);
		if (CreateDepth)
		{
			Ret &= d->DepthTex->CreateTexture2D(RenderCore::PF_DepthStencil, ETextureCreateFlags::TexCreate_DepthStencilTargetable, SizeX, SizeY);
		}

		return Ret;
	}

	core::vec2i D3D11TextureCube::GetSize() const
	{
		C_P(D3D11TextureCube);
		return d->Tex2D->GetSize();
	}

	uint32_t D3D11TextureCube::GetNumMips() const
	{
		C_P(D3D11TextureCube);
		return d->Tex2D->GetNumMips();
	}

	ID3D11Texture2D* D3D11TextureCube::GetNativeTex() const
	{
		C_P(D3D11TextureCube);
		return d->Tex2D->GetNativeTex();
	}

	std::map < uint32_t, std::vector< win32::com_ptr <ID3D11RenderTargetView>>> D3D11TextureCube::GetRTVS() const
	{
		C_P(const D3D11TextureCube);
		return d->Tex2D->GetRTVS();
	}

	std::map < uint32_t, std::vector< win32::com_ptr <ID3D11RenderTargetView>>>& D3D11TextureCube::GetRTVS()
	{
		C_P(D3D11TextureCube);
		return d->Tex2D->GetRTVS();
	}

	ID3D11ShaderResourceView* D3D11TextureCube::GetSRV() const
	{
		C_P(D3D11TextureCube);
		return d->Tex2D->GetSRV();
	}

	std::shared_ptr<D3D11Texture2D> D3D11TextureCube::GetDepthTex() const
	{
		C_P(D3D11TextureCube);
		return d->DepthTex;
	}

}
