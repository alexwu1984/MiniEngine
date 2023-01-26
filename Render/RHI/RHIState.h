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
}