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
		static std::shared_ptr<RHISamplerState> BoderLinerSampler;
		static std::shared_ptr<RHISamplerState> ClampPointSampler;
		static std::shared_ptr<RHISamplerState> WarpLinerSampler;
		static std::shared_ptr<RHISamplerState> MirrorLinerSampler;
		static std::shared_ptr<RHISamplerState> ShadowSampler;

		static std::shared_ptr<RHIRasterizerState> RasterizerStateCullNone;
		static std::shared_ptr<RHIRasterizerState> RasterizerStateCullBack;
		static std::shared_ptr<RHIRasterizerState> RasterizerStateCullFront;

		static std::shared_ptr<RHIBlendState> BlendDisable;
		static std::shared_ptr<RHIBlendState> BlendTraditional;
		/** SrcAlpha * Src.RGB + InvSrcAlpha * Dst.RGB on RT0-RT5 (scene+MRT+MaterialAux); use for deferred translucency (e.g. fur shells). */
		static std::shared_ptr<RHIBlendState> BlendDeferredTranslucentMRT;
		static std::shared_ptr<RHIBlendState> BlendOnAlphaOff;
		static std::shared_ptr<RHIBlendState> BlendOnAlphaOn;

		static std::shared_ptr<RHIDepthStencilState> DepthStateEnable;
		/** LessEqual depth test on, depth write off - for blended passes (e.g. fur shells) that must not punch depth holes. */
		static std::shared_ptr<RHIDepthStencilState> DepthStateLessEqualNoWrite;
		static std::shared_ptr<RHIDepthStencilState> DepthStateDisable;
	};
}