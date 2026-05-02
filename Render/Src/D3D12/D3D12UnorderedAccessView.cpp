#include "D3D12/D3D12UnorderedAccessView.h"
#include "D3D12/D3D12Texture2D.h"
#include "D3D12/D3D12Adapter.h"
#include "D3D12/D3D12WindowDevice.h"

namespace RenderCore
{
	struct D3D12UnorderedAccessViewPrivate
	{
		std::shared_ptr<D3D12Texture2D> Tex2D;
	};

	D3D12UnorderedAccessView::D3D12UnorderedAccessView(std::weak_ptr<FD3D12Adapter> InParentAdapter)
		:FD3D12AdapterChild(InParentAdapter)
		,d_ptr(new D3D12UnorderedAccessViewPrivate())
	{

	}

	D3D12UnorderedAccessView::~D3D12UnorderedAccessView()
	{
		delete d_ptr;
	}

	bool D3D12UnorderedAccessView::CreateFromTexture(std::shared_ptr<RHITexture2D> Tex2D, uint32_t MipLevel)
	{
		C_P(D3D12UnorderedAccessView);
		d->Tex2D = std::static_pointer_cast<D3D12Texture2D>(Tex2D);
		return d->Tex2D.get();
	}

	std::shared_ptr<RHITexture2D> D3D12UnorderedAccessView::GetTexture2D() const
	{
		C_P(const D3D12UnorderedAccessView);
		return d->Tex2D;
	}

	D3D12_CPU_DESCRIPTOR_HANDLE D3D12UnorderedAccessView::GetUAV() const
	{
		C_P(const D3D12UnorderedAccessView);
		if (d->Tex2D)
			return d->Tex2D->GetUAV();
		return { D3D12_GPU_VIRTUAL_ADDRESS_NULL };
	}

}