#pragma once
#include "core/inc.h"

namespace RenderCore
{
	class RHICommandContext;
	class DynamicRHI;
	class RHIRenderTarget;
	class RHITexture2D;
	class RHIUnorderedAccessView;
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

		void InitResource(int32_t MipLevel);
		void Draw(RenderCore::RHICommandContext& RHIContext, std::shared_ptr<RenderCore::RHITexture2D> SrcTex);
		void Draw(RenderCore::RHICommandContext& RHIContext, std::shared_ptr<RenderCore::RHITexture2D> SrcTex,int32_t IndexMip);
	private:
		BlurPSPrivate* d_ptr = nullptr;
	};

	struct BlurCSPrivate;
	class BlurCS
	{
	public:
		BlurCS(RenderCore::DynamicRHI* RHI);
		~BlurCS();

		void InitResource();
		void Dispatch(RenderCore::RHICommandContext& RHIContext, std::shared_ptr<RenderCore::RHITexture2D> SrcTex, 
					 std::shared_ptr<RenderCore::RHIUnorderedAccessView> Target);

	private:
		BlurCSPrivate* d_ptr = nullptr;
	};
}