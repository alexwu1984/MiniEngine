#pragma once
#include "RHI/RHI.h"

namespace RenderCore
{
	class RHISamplerState
	{
	public:
		RHISamplerState() = default;
		virtual ~RHISamplerState() {}

		virtual bool CreateSamplerState(const SamplerStateInitializerRHI& Initializer) = 0;
	};

	class RHIRasterizerState
	{
	public:
		RHIRasterizerState() = default;
		virtual ~RHIRasterizerState() {}

		virtual bool CreateRasterizerState(const RasterizerStateInitializerRHI& Initializer) = 0;
	};

	class RHIBlendState
	{
	public:
		RHIBlendState() = default;
		virtual ~RHIBlendState() {}
		virtual bool CreateBlendState(const BlendStateInitializerRHI& Initializer) = 0;
	};

	class RHIDepthStencilState
	{
	public:
		RHIDepthStencilState() = default;
		virtual ~RHIDepthStencilState() {}
		virtual bool CreateDepthStencilState(const DepthStencilStateInitializerRHI& Initializer) = 0;
	};
}