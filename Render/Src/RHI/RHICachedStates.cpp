#include "RHI/RHICachedStates.h"
#include "RHI/DynamicRHI.h"

namespace RenderCore
{
	std::shared_ptr<RHISamplerState> RHICachedStates::ClampLinerSampler;
	std::shared_ptr<RHISamplerState> RHICachedStates::ShadowSampler;
	std::shared_ptr<RHISamplerState> RHICachedStates::WarpLinerSampler;
	std::shared_ptr<RHISamplerState> RHICachedStates::MirrorLinerSampler;

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

	}

	void RHICachedStates::DestroyAll()
	{
		ClampLinerSampler = {};
		ShadowSampler = {};
		WarpLinerSampler = {};
		MirrorLinerSampler = {};
	}

}



