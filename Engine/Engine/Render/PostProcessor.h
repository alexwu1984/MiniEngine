#pragma once
#include "core/inc.h"

namespace RenderCore
{
	class RHICommandContext;
	class DynamicRHI;
}

namespace Engine
{
	struct PostProcessorPrivate;

	class PostProcessor
	{
	public:
		PostProcessor(RenderCore::DynamicRHI* RHI);
		~PostProcessor();

		void InitResource();
		void Draw(RenderCore::RHICommandContext& RHIContext);

	private:
		PostProcessorPrivate* d_ptr = nullptr;
	};
}