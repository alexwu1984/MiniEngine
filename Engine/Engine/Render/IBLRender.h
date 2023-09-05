#pragma once
#include "core/inc.h"

namespace RenderCore
{
	class RHICommandContext;
}

namespace Engine
{
	struct IBLRenderPrivate;

	class IBLRender
	{
	public:
		IBLRender();
		~IBLRender();

		void InitResource();

		void Draw(RenderCore::RHICommandContext& RHIContext);

	private:
		IBLRenderPrivate* d_ptr = nullptr;
	};
}