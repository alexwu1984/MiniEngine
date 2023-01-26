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
		static std::shared_ptr<RHISamplerState> WarpLinerSampler;
		static std::shared_ptr<RHISamplerState> MirrorLinerSampler;
		static std::shared_ptr<RHISamplerState> ShadowSampler;
	};
}