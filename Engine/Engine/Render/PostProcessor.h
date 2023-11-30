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

	class PostProcessor
	{
	public:
		PostProcessor(RenderCore::DynamicRHI* RHI);
		~PostProcessor();

		void InitResource();
		void Draw(RenderCore::RHICommandContext& RHIContext, std::shared_ptr<GBuffer> TargetBuffer, std::shared_ptr<RenderCore::RHIViewPort> ViewPort);
	private:
		void Tonemapping(RenderCore::RHICommandContext& RHIContext, std::shared_ptr<GBuffer> TargetBuffer);
	private:
		PostProcessorPrivate* d_ptr = nullptr;
	};
}