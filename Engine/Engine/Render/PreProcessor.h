#pragma once
#include "core/inc.h"

namespace RenderCore
{
	class RHICommandContext;
	class DynamicRHI;
}

namespace Engine
{
	struct PreProcessorPrivate;
	class PreProcessor
	{
	public:
		PreProcessor();
		~PreProcessor();

		void InitResource(RenderCore::DynamicRHI* RHI);
		void Draw(RenderCore::RHICommandContext& RHIContext);

	private:
		PreProcessorPrivate* d_ptr = nullptr;
	};
}
