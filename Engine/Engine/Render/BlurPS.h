#pragma once
#include "core/inc.h"

namespace RenderCore
{
	class RHICommandContext;
	class DynamicRHI;
	class RHIRenderTarget;
	class RHITexture2D;
}

namespace Engine
{
	struct BlurPSPrivate;
	class GBuffer;

	class BlurPS
	{
	public:
		BlurPS(RenderCore::DynamicRHI* RHI);
		~BlurPS();

		void InitResource();
		void Draw(RenderCore::RHICommandContext& RHIContext, std::shared_ptr<RenderCore::RHITexture2D> SrcTex , std::shared_ptr<GBuffer> TargetBuffer);
	private:
		void Draw(RenderCore::RHICommandContext& RHIContext, std::shared_ptr<RenderCore::RHITexture2D> SrcTex,int32_t IndexMip);
	private:
		BlurPSPrivate* d_ptr = nullptr;
	};
}