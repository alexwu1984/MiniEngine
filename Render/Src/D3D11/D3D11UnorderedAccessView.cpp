#include "D3D11/D3D11UnorderedAccessView.h"
#include "RHI/RHI.h"
#include "D3D11/D3D11RHI.h"
#include "D3D11/D3D11ReourceTraits.h"
#include "RHIPrivate/D3D11RHIPrivate.h"

namespace RenderCore
{
	struct D3D11UnorderedAccessViewPrivate
	{
		D3D11DynamicRHI* D3D11RHI;
		win32::com_ptr<ID3D11UnorderedAccessView> UnorderedAccessView;
		std::shared_ptr<RHITexture2D> Tex2D;
	};

	D3D11UnorderedAccessView::D3D11UnorderedAccessView(D3D11DynamicRHI* D3D11RHI)
		:d_ptr(new D3D11UnorderedAccessViewPrivate())
	{
		C_P(D3D11UnorderedAccessView);
		d->D3D11RHI = D3D11RHI;
	}

	D3D11UnorderedAccessView::~D3D11UnorderedAccessView()
	{
		delete d_ptr;
	}

	bool D3D11UnorderedAccessView::CreateFromTexture(std::shared_ptr<RHITexture2D> Tex2D, uint32_t MipLevel)
	{
		C_P(D3D11UnorderedAccessView);
		d->Tex2D = Tex2D;
		auto D3D11Tex = RHIResourceCast(Tex2D.get());
		D3D11_UNORDERED_ACCESS_VIEW_DESC UAVDesc{};
		UAVDesc.ViewDimension = D3D11_UAV_DIMENSION_TEXTURE2D;
		UAVDesc.Texture2D.MipSlice = MipLevel;
		UAVDesc.Format = FindShaderResourceDXGIFormat((DXGI_FORMAT)GPixelFormats[Tex2D->GetPixelFormat()].PlatformFormat, false);
		auto Device = d->D3D11RHI->GetDevice();
		VERIFYD3D11RESULT(Device->CreateUnorderedAccessView(D3D11Tex->GetNativeTex(), &UAVDesc, d->UnorderedAccessView.getpp()));
		return d->UnorderedAccessView.is_valid();
	}

	std::shared_ptr<RenderCore::RHITexture2D> D3D11UnorderedAccessView::GetTexture2D() const
	{
		C_P(const D3D11UnorderedAccessView);
		return d->Tex2D;
	}

	win32::com_ptr<ID3D11UnorderedAccessView> D3D11UnorderedAccessView::GetNativeUAV() const
	{
		C_P(const D3D11UnorderedAccessView);
		return d->UnorderedAccessView;
	}

}

