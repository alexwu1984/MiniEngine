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
		IBLRender(RenderCore::DynamicRHI* RHI);
		~IBLRender();

		void InitResource(RenderCore::DynamicRHI* RHI);
		void Draw(RenderCore::RHICommandContext& RHIContext);

	private:
		void InitShader();
	private:
		IBLRenderPrivate* d_ptr = nullptr;
	};
}