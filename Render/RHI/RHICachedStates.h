#pragma once
#include "RHI/RHIState.h"

namespace RenderCore
{
	class DynamicRHI;
	class RHICachedStates
	{
	public:
		RHICachedStates() = default;
		~RHICachedStates() = default;

		static void Initialize(DynamicRHI *RHI);
		static void DestroyAll();

	public:
		static std::shared_ptr<RHISamplerState> ClampLinerSampler;
		static std::shared_ptr<RHISamplerState> ClampPointSampler;
		static std::shared_ptr<RHISamplerState> WarpLinerSampler;
		static std::shared_ptr<RHISamplerState> MirrorLinerSampler;
		static std::shared_ptr<RHISamplerState> ShadowSampler;

		static std::shared_ptr<RHIRasterizerState> RasterizerStateCullNone;
		static std::shared_ptr<RHIRasterizerState> RasterizerStateCullBack;
		static std::shared_ptr<RHIRasterizerState> RasterizerStateCullFront;

		static std::shared_ptr<RHIBlendState> BlendDisable;
		static std::shared_ptr<RHIBlendState> BlendTraditional;
		static std::shared_ptr<RHIBlendState> BlendOnAlphaOff;
		static std::shared_ptr<RHIBlendState> BlendOnAlphaOn;

		static std::shared_ptr<RHIDepthStencilState> DepthStateEnable;
		static std::shared_ptr<RHIDepthStencilState> DepthStateDisable;
	};
}