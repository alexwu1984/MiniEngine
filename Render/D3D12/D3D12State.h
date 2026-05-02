#pragma once
#include "RHI/RHIState.h"
#include "RHIPrivate/D3D12RHIPrivate.h"

namespace RenderCore
{
	class D3D12SamplerState : public RHISamplerState
	{
	public:
		D3D12SamplerState() = default;
		virtual ~D3D12SamplerState() = default;

		virtual bool CreateSamplerState(const SamplerStateInitializerRHI& Initializer) override;
		const D3D12_STATIC_SAMPLER_DESC& GetSampleDesc() const;
	private:
		D3D12_STATIC_SAMPLER_DESC SamplerDesc;
	};

	class D3D12RasterizerState : public RHIRasterizerState
	{
	public:
		D3D12RasterizerState() = default;
		virtual ~D3D12RasterizerState() = default;
		virtual bool CreateRasterizerState(const RasterizerStateInitializerRHI& Initializer) override;
		const D3D12_RASTERIZER_DESC& GetRasterizerDesc() const;
	private:
		D3D12_RASTERIZER_DESC RasterizerDesc;
	};

	class D3D12BlendState : public RHIBlendState
	{
	public:
		D3D12BlendState() = default;
		virtual ~D3D12BlendState() = default;
		virtual bool CreateBlendState(const BlendStateInitializerRHI& Initializer) override;
		const D3D12_BLEND_DESC& GetBlendDesc() const;
	private:
		D3D12_BLEND_DESC BlendDesc;
	};

	class D3D12DepthStencilState : public RHIDepthStencilState
	{
	public:
		D3D12DepthStencilState() = default;
		virtual ~D3D12DepthStencilState() = default;
		virtual bool CreateDepthStencilState(const DepthStencilStateInitializerRHI& Initializer) override;
		const D3D12_DEPTH_STENCIL_DESC& GetDepthStencilDesc() const;
	private:
		D3D12_DEPTH_STENCIL_DESC DepthStencilDesc;
	};
}