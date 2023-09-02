#include "D3D11/D3D11TextureCube.h"
#include "D3D11/D3D11Texture2D.h"

namespace RenderCore
{
	struct D3D11TextureCubePrivate
	{
		D3D11DynamicRHI* D3D11RHI = nullptr;
		std::shared_ptr<D3D11Texture2D> Tex2D;
	};

	D3D11TextureCube::D3D11TextureCube(D3D11DynamicRHI* D3D11RHI)
		:d_ptr(new D3D11TextureCubePrivate())
	{
		C_P(D3D11TextureCube);
		d->D3D11RHI = D3D11RHI;
		d->Tex2D = std::make_shared<D3D11Texture2D>(D3D11RHI);
	}

	D3D11TextureCube::~D3D11TextureCube()
	{
		delete d_ptr;
	}

	bool D3D11TextureCube::CreateD3D11TextureCube(EPixelFormat Format, int32_t Flags, int32_t SizeX, int32_t SizeY)
	{
		C_P(D3D11TextureCube);
		return d->Tex2D->CreateD3D11Texture2D(Format, Flags, SizeX, SizeY, 6);
	}

	bool D3D11TextureCube::IsMultisampled() const
	{
		C_P(D3D11TextureCube);
		return d->Tex2D->IsMultisampled();
	}

	core::vec2i D3D11TextureCube::GetSize() const
	{
		C_P(D3D11TextureCube);
		return d->Tex2D->GetSize();
	}

}
