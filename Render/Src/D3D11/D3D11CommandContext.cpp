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
		:Impl(std::make_shared<D3D11CommandContextP>())
	{
		Impl->D3D11RHI = D3D11RHI;
	}

	D3D11CommandContext::~D3D11CommandContext()
	{

	}

	void D3D11CommandContext::SetViewPort(int32_t TopLeftX, int32_t TopLeftY, int32_t SizeX, int32_t SizeY)
	{
		Impl->D3D11RHI->GetStateCache().CurrentNumberOfViewports = 1;

		auto& ViewPort = Impl->D3D11RHI->GetStateCache().CurrentViewport[0];
		ViewPort.Width = static_cast<float>(SizeX);
		ViewPort.Height = static_cast<float>(SizeY);
		ViewPort.MinDepth = 0.0f;
		ViewPort.MaxDepth = 1.0f;
		ViewPort.TopLeftX = static_cast<float>(TopLeftX);
		ViewPort.TopLeftY = static_cast<float>(TopLeftY);

		Impl->D3D11RHI->GetDeviceContext()->RSSetViewports(Impl->D3D11RHI->GetStateCache().CurrentNumberOfViewports, &ViewPort);
	}

	void D3D11CommandContext::SetRenderTarget(std::shared_ptr< RHITexture2D> Tex, std::shared_ptr< RHITexture2D> Depth)
	{
		auto TexRHI = RHIResourceCast(Tex.get());
		auto DepthRHI = RHIResourceCast(Depth.get());
		if (TexRHI)
		{
			auto RenderTargetView = TexRHI->GetRTV();
			Impl->D3D11RHI->GetDeviceContext()->OMSetRenderTargets(1, &RenderTargetView, DepthRHI ? DepthRHI->GetDSV() : nullptr);
		}
		else
		{
			Impl->D3D11RHI->GetDeviceContext()->OMSetRenderTargets(0, nullptr, DepthRHI ? DepthRHI->GetDSV() : nullptr);
		}
	}

	void D3D11CommandContext::SetRenderTarget(std::shared_ptr< RHIRenderTarget> RenderTarget)
	{
		auto RenderTargetRHI = RHIResourceCast(RenderTarget.get());
		if (RenderTargetRHI)
		{
			auto RTV = RenderTargetRHI->GetRTV();
			Impl->D3D11RHI->GetDeviceContext()->OMSetRenderTargets(1, &RTV, RenderTargetRHI->GetDSV());
		}
		else
		{
			Impl->D3D11RHI->GetDeviceContext()->OMSetRenderTargets(0, nullptr, nullptr);
		}
	}

	void D3D11CommandContext::Clear(std::shared_ptr< RHIRenderTarget> RenderTarget, const core::FLinearColor& Color, float Depth /*= 1.0f*/, uint8_t Stencil /*= 0*/)
	{
		auto RenderTargetRHI = RHIResourceCast(RenderTarget.get());
		auto DeviceContex = Impl->D3D11RHI->GetDeviceContext();

		auto RTV = RenderTargetRHI->GetRTV();
		if (RTV != NULL)
		{
			DeviceContex->ClearRenderTargetView(RTV, &Color.R);
		}

		auto DSV = RenderTargetRHI->GetDSV();
		if (DSV != NULL)
		{
			DeviceContex->ClearDepthStencilView(DSV, D3D11_CLEAR_DEPTH, Depth, Stencil);
		}
	}

	void D3D11CommandContext::RHISetShaderSampler(EShaderFrequency ShaderType, uint32_t SamplerIndex, std::shared_ptr< RHISamplerState> NewState)
	{
		D3D11SamplerState* SamplerStateRHI = RHIResourceCast(NewState.get());
		D3D11StateCacheBase& StateCache = Impl->D3D11RHI->GetStateCache();

		switch (ShaderType)
		{
		case SF_Vertex:
			StateCache.SetSamplerState<SF_Vertex>(SamplerStateRHI->GetNativeSampleState(), SamplerIndex);
			break;
		case SF_Hull:
			StateCache.SetSamplerState<SF_Hull>(SamplerStateRHI->GetNativeSampleState(), SamplerIndex);
			break;
		case SF_Domain:
			StateCache.SetSamplerState<SF_Domain>(SamplerStateRHI->GetNativeSampleState(), SamplerIndex);
			break;
		case SF_Pixel:
			StateCache.SetSamplerState<SF_Pixel>(SamplerStateRHI->GetNativeSampleState(), SamplerIndex);
			break;
		case SF_Geometry:
			StateCache.SetSamplerState<SF_Geometry>(SamplerStateRHI->GetNativeSampleState(), SamplerIndex);
			break;
		case SF_Compute:
			StateCache.SetSamplerState<SF_Compute>(SamplerStateRHI->GetNativeSampleState(), SamplerIndex);
			break;
		default:
			Assert(false);
			break;
		}
		
	}

	void D3D11CommandContext::RHISetRasterizerState(std::shared_ptr<RHIRasterizerState> NewStateRHI)
	{
		D3D11RasterizerState* RasterizerStateRHI = RHIResourceCast(NewStateRHI.get());
		D3D11StateCacheBase& StateCache = Impl->D3D11RHI->GetStateCache();
		StateCache.SetRasterizerState(RasterizerStateRHI->GetNativeRasterizerState());
	}

	void D3D11CommandContext::RHISetBlendState(std::shared_ptr<RHIBlendState> NewState, const core::FLinearColor& BlendFactor)
	{
		D3D11BlendState* BlendStateRHI = RHIResourceCast(NewState.get());
		D3D11StateCacheBase& StateCache = Impl->D3D11RHI->GetStateCache();
		StateCache.SetBlendState(BlendStateRHI->GetNativeBlendState(), (const float*)&BlendFactor, 0xffffffff);
	}

	void D3D11CommandContext::RHISetBlendFactor(const core::FLinearColor& BlendFactor)
	{
		D3D11StateCacheBase& StateCache = Impl->D3D11RHI->GetStateCache();
		StateCache.SetBlendFactor((const float*)&BlendFactor, 0xffffffff);
	}

	void D3D11CommandContext::RHISetDepthStencilState(std::shared_ptr< RHIDepthStencilState> NewState, uint32_t StencilRef)
	{
		D3D11DepthStencilState* DepthStencilStateRHI = RHIResourceCast(NewState.get());
		D3D11StateCacheBase& StateCache = Impl->D3D11RHI->GetStateCache();
		StateCache.SetDepthStencilState(DepthStencilStateRHI->GetNativeDepthStencilState(), StencilRef);
	}

	void D3D11CommandContext::RHISetStencilRef(uint32_t StencilRef)
	{
		D3D11StateCacheBase& StateCache = Impl->D3D11RHI->GetStateCache();
		StateCache.SetStencilRef(StencilRef);
	}

}