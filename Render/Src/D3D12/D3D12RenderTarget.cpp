#include "D3D12/D3D12RenderTarget.h"
#include "D3D12/D3D12CommandContext.h"
#include "D3D12/D3D12Texture2D.h"
#include "D3D12/D3D12Adapter.h"
#include "D3D12/D3D12WindowDevice.h"

namespace RenderCore
{
	struct D3D12RenderTargetPrivate
	{
		std::shared_ptr<D3D12Texture2D> Tex2D;
		std::shared_ptr<D3D12Texture2D> DepthTex;
		core::vec2i Size;
	};

	D3D12RenderTarget::D3D12RenderTarget(std::weak_ptr<FD3D12Adapter> InParentAdapter)
		:FD3D12AdapterChild(InParentAdapter)
		,d_ptr(new D3D12RenderTargetPrivate)
	{

	}

	D3D12RenderTarget::~D3D12RenderTarget()
	{
		delete d_ptr;
	}

	bool D3D12RenderTarget::Create(EPixelFormat Format, int32_t SizeX, int32_t SizeY, uint32_t NumMips, bool IsMultiSampled, bool CreateDepth)
	{
		C_P(D3D12RenderTarget);
		d->Size = core::vec2i(SizeX, SizeY);
		d->Tex2D = std::make_shared<D3D12Texture2D>(GetParentAdapter());
		int32_t Flags = ETextureCreateFlags::TexCreate_RenderTargetable | ETextureCreateFlags::TexCreate_ShaderResource;
		//if (NumMips > 1)
		//{
		//	Flags |= TexCreate_GenerateMipCapable;
		//}
		if (IsMultiSampled)
		{
			Flags |= ETextureCreateFlags::TexCreate_MSAA;
		}

		if (!d->Tex2D->CreateTexture2D(Format, Flags, SizeX, SizeY, 1, NumMips))
		{
			return false;
		}
		if (CreateDepth)
		{
			d->DepthTex = std::make_shared<D3D12Texture2D>(GetParentAdapter());
			Flags = ETextureCreateFlags::TexCreate_DepthStencilTargetable;
			if (IsMultiSampled)
			{
				Flags |= ETextureCreateFlags::TexCreate_MSAA;
			}

			return d->DepthTex->CreateTexture2D(EPixelFormat::PF_DepthStencil, Flags, SizeX, SizeY);
		}
		return true;
	}

	core::vec2i D3D12RenderTarget::GetSize() const
	{
		C_P(const D3D12RenderTarget);
		return d->Size;
	}

	void D3D12RenderTarget::Bind()
	{
		C_P(D3D12RenderTarget);
		GetParentAdapter()->GetDevice()->GetDefaultCommandContext()->SetRenderTarget(d->Tex2D, d->DepthTex);
	}

	void D3D12RenderTarget::UnBind()
	{
		GetParentAdapter()->GetDevice()->GetDefaultCommandContext()->SetRenderTarget(nullptr, nullptr);
	}

}