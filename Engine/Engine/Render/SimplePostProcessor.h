#pragma once
#include "core/inc.h"

namespace RenderCore
{
	class RHICommandContext;
	class DynamicRHI;
	class RHIViewPort;
}

namespace Engine
{
	struct PostProcessorPrivate;
	class GBuffer;

	class SimplePostProcessor
	{
	public:
		virtual ~SimplePostProcessor() {}

		virtual void Draw(RenderCore::RHICommandContext& RHIContext, std::shared_ptr<RenderCore::RHIViewPort> ViewPort,float DeltaTime) = 0;

	};
}