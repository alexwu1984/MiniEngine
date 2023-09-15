#pragma once
#include "core/inc.h"

namespace RenderCore
{
	class RHICommandContext;
	class DynamicRHI;
}

namespace Engine
{
	struct IBLRenderPrivate;

	class IBLRender
	{
	public:
		IBLRender();
		~IBLRender();

		void InitResource(RenderCore::DynamicRHI* RHI);

		void Draw(RenderCore::RHICommandContext& RHIContext);

	private:
		IBLRenderPrivate* d_ptr = nullptr;
	};
}