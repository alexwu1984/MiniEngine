#pragma once
#include "core/inc.h"

namespace RenderCore
{
	class RHICommandContext;
	class DynamicRHI;
	class RHIRenderTarget;
}

namespace Engine
{
	struct BlurPSPrivate;
	class GBuffer;

	class BlurPS
	{
		BlurPS(RenderCore::DynamicRHI* RHI);
		~BlurPS();

		void InitResource();
		void Draw(RenderCore::RHICommandContext& RHIContext, std::shared_ptr<GBuffer> TargetBuffer);
	private:
		BlurPSPrivate* d_ptr = nullptr;
	};
}