#pragma once
#include "core/inc.h"

namespace RenderCore
{
	class RHICommandContext;
}

namespace Engine
{
	struct PreProcessPrivate;
	class PreProcessor
	{
	public:
		PreProcessor();
		~PreProcessor();

		void InitResource();
		void Draw(RenderCore::RHICommandContext& RHIContext);
	};
}
