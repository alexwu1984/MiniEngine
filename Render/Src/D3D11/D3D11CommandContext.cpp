#include "D3D11/D3D11CommandContext.h"
#include "RHIPrivate/D3D11RHIPrivate.h"
#include "RHIPrivate/D3D11StateCachePrivate.h"
#include "D3D11/D3D11RHI.h"
#include "D3D11/D3D11ReourceTraits.h"

namespace RenderCore
{
	struct D3D11CommandContextP
	{
		D3D11DynamicRHI* D3D11RHI = nullptr;
	};

	D3D11CommandContext::D3D11CommandContext(D3D11DynamicRHI* D3D11RHI)
		:Data(std::make_shared<D3D11CommandContextP>())
	{
		Data->D3D11RHI = D3D11RHI;
	}

	D3D11CommandContext::~D3D11CommandContext()
	{

	}

	void D3D11CommandContext::SetViewPort(int32_t TopLeftX, int32_t TopLeftY, int32_t SizeX, int32_t SizeY)
	{
		Data->D3D11RHI->GetStateCache().CurrentNumberOfViewports = 1;

		auto& ViewPort = Data->D3D11RHI->GetStateCache().CurrentViewport[0];
		ViewPort.Width = static_cast<float>(SizeX);
		ViewPort.Height = static_cast<float>(SizeY);
		ViewPort.MinDepth = 0.0f;
		ViewPort.MaxDepth = 1.0f;
		ViewPort.TopLeftX = static_cast<float>(TopLeftX);
		ViewPort.TopLeftY = static_cast<float>(TopLeftY);

		Data->D3D11RHI->GetDeviceContext()->RSSetViewports(Data->D3D11RHI->GetStateCache().CurrentNumberOfViewports, &ViewPort);
	}

	void D3D11CommandContext::SetRenderTarget(std::shared_ptr< RHITexture2D> Tex, std::shared_ptr< RHITexture2D> Depth)
	{
		auto TexRHI = RHIResourceCast(Tex.get());
		auto DepthRHI = RHIResourceCast(Depth.get());
		if (TexRHI)
		{
			auto RenderTargetView = TexRHI->GetRTV();
			Data->D3D11RHI->GetDeviceContext()->OMSetRenderTargets(1, &RenderTargetView, DepthRHI ? DepthRHI->GetDSV() : nullptr);
		}
		else
		{
			Data->D3D11RHI->GetDeviceContext()->OMSetRenderTargets(0, nullptr, DepthRHI ? DepthRHI->GetDSV() : nullptr);
		}
	}

	void D3D11CommandContext::SetRenderTarget(std::shared_ptr< RHIRenderTarget> RenderTarget)
	{
		auto RenderTargetRHI = RHIResourceCast(RenderTarget.get());
		if (RenderTargetRHI)
		{
			auto RTV = RenderTargetRHI->GetRTV();
			Data->D3D11RHI->GetDeviceContext()->OMSetRenderTargets(1, &RTV, RenderTargetRHI->GetDSV());
		}
		else
		{
			Data->D3D11RHI->GetDeviceContext()->OMSetRenderTargets(0, nullptr, nullptr);
		}
	}

	void D3D11CommandContext::Clear(std::shared_ptr< RHIRenderTarget> RenderTarget, const math::Vector4 Color, float Depth /*= 1.0f*/, uint8_t Stencil /*= 0*/)
	{
		auto RenderTargetRHI = RHIResourceCast(RenderTarget.get());

		auto RTV = RenderTargetRHI->GetRTV();
		if (RTV != NULL)
		{
			Data->D3D11RHI->GetDeviceContext()->ClearRenderTargetView(RTV, &Color[0]);
		}

		auto DSV = RenderTargetRHI->GetDSV();
		if (DSV != NULL)
		{
			Data->D3D11RHI->GetDeviceContext()->ClearDepthStencilView(DSV, D3D11_CLEAR_DEPTH, Depth, Stencil);
		}
	}

}