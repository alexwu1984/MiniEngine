#include "RHI/RHICachedStates.h"
#include "RHI/DynamicRHI.h"

namespace RenderCore
{
	std::shared_ptr<RHISamplerState> RHICachedStates::ClampLinerSampler;
	std::shared_ptr<RHISamplerState> RHICachedStates::ShadowSampler;
	std::shared_ptr<RHISamplerState> RHICachedStates::WarpLinerSampler;
	std::shared_ptr<RHISamplerState> RHICachedStates::MirrorLinerSampler;

	std::shared_ptr<RHIRasterizerState> RHICachedStates::RasterizerStateCullNone;
	std::shared_ptr<RHIRasterizerState> RHICachedStates::RasterizerStateCullBack;
	std::shared_ptr<RHIRasterizerState> RHICachedStates::RasterizerStateCullFront;

	std::shared_ptr<RHIBlendState> RHICachedStates::BlendDisable;
	std::shared_ptr<RHIBlendState> RHICachedStates::BlendAlphaOff;
	std::shared_ptr<RHIBlendState> RHICachedStates::BlendAlphaOn;

	void RHICachedStates::Initialize(DynamicRHI* RHI)
	{
		SamplerStateInitializerRHI ClampParam(ESamplerFilter::SF_Bilinear, ESamplerAddressMode::AM_Clamp, ESamplerAddressMode::AM_Clamp, ESamplerAddressMode::AM_Clamp);
		ClampLinerSampler = RHI->RHICreateSampleState(ClampParam);

		SamplerStateInitializerRHI ShadowParam(ESamplerFilter::SF_Point, ESamplerAddressMode::AM_Border, ESamplerAddressMode::AM_Border, ESamplerAddressMode::AM_Border);
		ShadowSampler = RHI->RHICreateSampleState(ShadowParam);

		SamplerStateInitializerRHI WrapParam(ESamplerFilter::SF_Bilinear);
		WarpLinerSampler = RHI->RHICreateSampleState(WrapParam);

		SamplerStateInitializerRHI MirrorParam(ESamplerFilter::SF_Bilinear, ESamplerAddressMode::AM_Mirror, ESamplerAddressMode::AM_Mirror, ESamplerAddressMode::AM_Mirror);
		MirrorLinerSampler = RHI->RHICreateSampleState(MirrorParam);

		RasterizerStateInitializerRHI CullNoneParam;
		CullNoneParam.CullMode = ERasterizerCullMode::CM_None;
		RasterizerStateCullNone = RHI->RHICreateRasterizerState(CullNoneParam);

		RasterizerStateInitializerRHI CullBackParam;
		RasterizerStateCullBack = RHI->RHICreateRasterizerState(CullBackParam);
		
		RasterizerStateInitializerRHI CullFrontParam;
		RasterizerStateCullFront = RHI->RHICreateRasterizerState(CullFrontParam);

		BlendStateInitializerRHI BlendDisableParam;
		BlendDisable = RHI->RHICreateBlendState(BlendDisableParam);

		BlendStateInitializerRHI BlendAlphaOffParam;
		BlendAlphaOffParam.RenderTargets[0].AlphaSrcBlend = EBlendFactor::BF_One;
		BlendAlphaOffParam.RenderTargets[0].AlphaDestBlend = EBlendFactor::BF_One;
		BlendAlphaOffParam.RenderTargets[0].ColorSrcBlend = EBlendFactor::BF_SourceAlpha;
		BlendAlphaOffParam.RenderTargets[0].ColorDestBlend = EBlendFactor::BF_InverseSourceAlpha;
		BlendAlphaOff = RHI->RHICreateBlendState(BlendAlphaOffParam);

		BlendStateInitializerRHI BlendAlphaOnParam = BlendAlphaOffParam;
		BlendAlphaOnParam.RenderTargets[0].AlphaSrcBlend = EBlendFactor::BF_SourceAlpha;
		BlendAlphaOnParam.RenderTargets[0].AlphaDestBlend = EBlendFactor::BF_InverseSourceAlpha;
		BlendAlphaOn = RHI->RHICreateBlendState(BlendAlphaOnParam);
	}

	void RHICachedStates::DestroyAll()
	{
		ClampLinerSampler = {};
		ShadowSampler = {};
		WarpLinerSampler = {};
		MirrorLinerSampler = {};

		RasterizerStateCullNone = {};
		RasterizerStateCullBack = {};
		RasterizerStateCullFront = {};

		BlendDisable = {};
		BlendAlphaOff = {};
		BlendAlphaOn = {};
	}

}



