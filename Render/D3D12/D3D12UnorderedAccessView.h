#pragma once
#include "RHI/RHIUnorderedAccessView.h"
#include "D3D12/D3D12RHICommon.h"

namespace RenderCore
{
	struct D3D12UnorderedAccessViewPrivate;

	class D3D12UnorderedAccessView : public RHIUnorderedAccessView, public FD3D12AdapterChild
	{
	public:
		D3D12UnorderedAccessView(std::weak_ptr<FD3D12Adapter> InParentAdapter);
		~D3D12UnorderedAccessView();

		virtual bool CreateFromTexture(std::shared_ptr<RHITexture2D> Tex2D, uint32_t MipLevel) override;
		virtual std::shared_ptr<RHITexture2D> GetTexture2D() const override;
		D3D12_CPU_DESCRIPTOR_HANDLE GetUAV() const;

	private:
		D3D12UnorderedAccessViewPrivate* d_ptr;
	};
}