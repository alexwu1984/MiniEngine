#pragma once
#include "core/inc.h"
#include "math/vector2.h"

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
	class SceneTextures;

	class BlurPS
	{
	public:
		BlurPS(RenderCore::DynamicRHI* RHI);
		~BlurPS();

		void InitResource(int32_t MipLevel);
		void Draw(RenderCore::RHICommandContext& RHIContext, std::shared_ptr<RenderCore::RHITexture2D> SrcTex);
		std::shared_ptr<RenderCore::RHITexture2D> GetResult() const;
	private:
		void Draw(RenderCore::RHICommandContext& RHIContext, std::shared_ptr<RenderCore::RHITexture2D> InTex, std::shared_ptr<RenderCore::RHIRenderTarget> Intermediate, std::shared_ptr<RenderCore::RHIRenderTarget> OutTex, uint32_t iterations = 2);
		void Step(RenderCore::RHICommandContext& RHIContext, std::shared_ptr<RenderCore::RHITexture2D> InTex, std::shared_ptr<RenderCore::RHIRenderTarget> OutTex, math::Vector2 Dir);
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