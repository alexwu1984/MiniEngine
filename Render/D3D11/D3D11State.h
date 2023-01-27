#pragma once
#include "RHI/RHIState.h"
#include "RHIPrivate/D3D11RHIDeclare.h"

namespace RenderCore
{
	class D3D11DynamicRHI;
	struct D3D11SamplerStateP;
	
	class D3D11SamplerState final : public RHISamplerState
	{
	public:
		D3D11SamplerState(D3D11DynamicRHI *D3D11RHI);
		virtual ~D3D11SamplerState();

		virtual bool CreateSamplerState(const SamplerStateInitializerRHI& Initializer) override ;
		ID3D11SamplerState* GetNativeSampleState() const;

	private:
		std::shared_ptr< D3D11SamplerStateP> Impl;
	};

	struct D3D11RasterizerStateP;

	class D3D11RasterizerState : public RHIRasterizerState
	{
	public:
		D3D11RasterizerState(D3D11DynamicRHI* D3D11RHI);
		virtual ~D3D11RasterizerState();
		virtual bool CreateRasterizerState(const RasterizerStateInitializerRHI& Initializer) override;
		ID3D11RasterizerState* GetNativeRasterizerState() const;
	private:
		std::shared_ptr< D3D11RasterizerStateP> Impl;
	};

	struct D3D11BlendStateP;

	class D3D11BlendState : public RHIBlendState
	{
	public:
		D3D11BlendState(D3D11DynamicRHI* D3D11RHI);
		virtual ~D3D11BlendState();
		virtual bool CreateBlendState(const BlendStateInitializerRHI& Initializer) override;
		ID3D11BlendState* GetNativeBlendState() const;
	private:
		std::shared_ptr< D3D11BlendStateP> Impl;
	};

	struct D3D11DepthStencilStateP;
	class D3D11DepthStencilState : public RHIDepthStencilState
	{
	public:
		D3D11DepthStencilState(D3D11DynamicRHI* D3D11RHI);
		virtual ~D3D11DepthStencilState();
		virtual bool CreateDepthStencilState(const DepthStencilStateInitializerRHI& Initializer) override;
		ID3D11DepthStencilState* GetNativeDepthStencilState() const;
	private:
		std::shared_ptr<D3D11DepthStencilStateP> Impl;
	};
}