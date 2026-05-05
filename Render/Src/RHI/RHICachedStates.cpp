#include "RHI/RHICachedStates.h"
#include "RHI/RHIStaticState.h"

namespace RenderCore
{
	std::shared_ptr<RHISamplerState> RHICachedStates::ClampLinerSampler;
	std::shared_ptr<RHISamplerState> RHICachedStates::BoderLinerSampler;
	std::shared_ptr<RHISamplerState> RHICachedStates::ClampPointSampler;
	std::shared_ptr<RHISamplerState> RHICachedStates::ShadowSampler;
	std::shared_ptr<RHISamplerState> RHICachedStates::WarpLinerSampler;
	std::shared_ptr<RHISamplerState> RHICachedStates::MirrorLinerSampler;

	std::shared_ptr<RHIRasterizerState> RHICachedStates::RasterizerStateCullNone;
	std::shared_ptr<RHIRasterizerState> RHICachedStates::RasterizerStateCullBack;
	std::shared_ptr<RHIRasterizerState> RHICachedStates::RasterizerStateCullFront;

	std::shared_ptr<RHIBlendState> RHICachedStates::BlendDisable;
	std::shared_ptr<RHIBlendState> RHICachedStates::BlendTraditional;
	std::shared_ptr<RHIBlendState> RHICachedStates::BlendDeferredTranslucentMRT;
	std::shared_ptr<RHIBlendState> RHICachedStates::BlendOnAlphaOff;
	std::shared_ptr<RHIBlendState> RHICachedStates::BlendOnAlphaOn;

	std::shared_ptr<RHIDepthStencilState> RHICachedStates::DepthStateEnable;
	std::shared_ptr<RHIDepthStencilState> RHICachedStates::DepthStateLessEqualNoWrite;
	std::shared_ptr<RHIDepthStencilState> RHICachedStates::DepthStateDisable;

	void RHICachedStates::Initialize(DynamicRHI* RHI)
	{
		ClampLinerSampler = TStaticSamplerState<SF_Bilinear>::CreateRHI(RHI);
		BoderLinerSampler = TStaticSamplerState<SF_Bilinear, AM_Border, AM_Border, AM_Border>::CreateRHI(RHI);
		ClampPointSampler = TStaticSamplerState<>::CreateRHI(RHI);
		ShadowSampler = TStaticSamplerState<SF_Point, AM_Border, AM_Border, AM_Border>::CreateRHI(RHI);
		WarpLinerSampler = TStaticSamplerState<SF_Bilinear, AM_Wrap, AM_Wrap, AM_Wrap>::CreateRHI(RHI);
		MirrorLinerSampler = TStaticSamplerState<SF_Bilinear, AM_Mirror, AM_Mirror, AM_Mirror>::CreateRHI(RHI);

		RasterizerStateCullNone = TStaticRasterizerState<>::CreateRHI(RHI);
		RasterizerStateCullBack = TStaticRasterizerState<FM_Solid, CM_CW>::CreateRHI(RHI);
		RasterizerStateCullFront = TStaticRasterizerState<FM_Solid, CM_CCW>::CreateRHI(RHI);

		BlendDisable = TStaticBlendState<>::CreateRHI(RHI);
		BlendTraditional = TStaticBlendState<CW_RGBA, BO_Add, BF_SourceAlpha, BF_InverseSourceAlpha, BO_Add, BF_One, BF_InverseSourceAlpha>::CreateRHI(RHI);
		BlendDeferredTranslucentMRT = TStaticBlendState<
			CW_RGBA, BO_Add, BF_SourceAlpha, BF_InverseSourceAlpha, BO_Add, BF_One, BF_InverseSourceAlpha,
			CW_RGBA, BO_Add, BF_SourceAlpha, BF_InverseSourceAlpha, BO_Add, BF_One, BF_InverseSourceAlpha,
			CW_RGBA, BO_Add, BF_SourceAlpha, BF_InverseSourceAlpha, BO_Add, BF_One, BF_InverseSourceAlpha,
			CW_RGBA, BO_Add, BF_SourceAlpha, BF_InverseSourceAlpha, BO_Add, BF_One, BF_InverseSourceAlpha,
			CW_RGBA, BO_Add, BF_SourceAlpha, BF_InverseSourceAlpha, BO_Add, BF_One, BF_InverseSourceAlpha,
			CW_RGBA, BO_Add, BF_SourceAlpha, BF_InverseSourceAlpha, BO_Add, BF_One, BF_InverseSourceAlpha>::CreateRHI(RHI);
		BlendOnAlphaOff = TStaticBlendState<CW_RGBA, BO_Add, BF_SourceAlpha, BF_InverseSourceAlpha, BO_Add, BF_One, BF_One>::CreateRHI(RHI);
		BlendOnAlphaOn = TStaticBlendState<CW_RGBA, BO_Add, BF_SourceAlpha, BF_InverseSourceAlpha, BO_Add, BF_SourceAlpha, BF_InverseSourceAlpha>::CreateRHI(RHI);

		DepthStateDisable = TStaticDepthStencilState<false, CF_Always>::CreateRHI(RHI);
		DepthStateEnable = TStaticDepthStencilState<true, CF_LessEqual>::CreateRHI(RHI);
		DepthStateLessEqualNoWrite = TStaticDepthStencilState<false, CF_LessEqual>::CreateRHI(RHI);

	}

	void RHICachedStates::DestroyAll()
	{
		ClampLinerSampler = {};
		BoderLinerSampler = {};
		ClampPointSampler = {};
		ShadowSampler = {};
		WarpLinerSampler = {};
		MirrorLinerSampler = {};

		RasterizerStateCullNone = {};
		RasterizerStateCullBack = {};
		RasterizerStateCullFront = {};

		BlendDisable = {};
		BlendTraditional = {};
		BlendDeferredTranslucentMRT = {};
		BlendOnAlphaOff = {};
		BlendOnAlphaOn = {};

		DepthStateDisable = {};
		DepthStateEnable = {};
		DepthStateLessEqualNoWrite = {};
	}

}



