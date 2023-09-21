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

		void InitResource();
		void Draw(RenderCore::RHICommandContext& RHIContext);

	private:
		void InitShader();
		void RenderCube(RenderCore::RHICommandContext& RHIContext);
	private:
		IBLRenderPrivate* d_ptr = nullptr;
	};
}