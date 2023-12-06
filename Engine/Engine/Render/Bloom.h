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
	struct BloomPrivate;
	class GBuffer;

	class Bloom
	{
	public:
		Bloom(RenderCore::DynamicRHI* RHI);
		~Bloom();

		void InitResource();
		void Draw(RenderCore::RHICommandContext& RHIContext, std::shared_ptr<GBuffer> TargetBuffer);

	private:
		BloomPrivate* d_ptr = nullptr;
	};
}
