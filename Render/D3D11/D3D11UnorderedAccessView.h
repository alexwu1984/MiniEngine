#pragma once
#include "RHI/RHIUnorderedAccessView.h"

namespace RenderCore
{
	struct D3D11UnorderedAccessViewPrivate;
	class D3D11DynamicRHI;

	class D3D11UnorderedAccessView : public RHIUnorderedAccessView
	{
	public:
		D3D11UnorderedAccessView(D3D11DynamicRHI* D3D11RHI);
		~D3D11UnorderedAccessView();

		virtual bool CreateFromTexture(std::shared_ptr<RHITexture2D> Tex2D, uint32_t MipLevel) override;

	private:
		D3D11UnorderedAccessViewPrivate* d_ptr;
	};
}