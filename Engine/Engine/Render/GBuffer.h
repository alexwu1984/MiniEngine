#pragma once
#include "core/inc.h"

namespace RenderCore
{
	class RHICommandContext;
	class DynamicRHI;
	class RHITexture2D;
}

namespace Engine
{
	struct GBufferPrivate;

	enum GBufferFlagBits
	{
		GBUFFER_NONE = 0,
		GBUFFER_DEPTH = 1,
		GBUFFER_MOTION_VECTORS = 2,
		GBUFFER_SCENE_COLOR = 8,
		GBUFFER_NORMAL_BUFFER = 16
	};

	class GBuffer
	{
	public:
		GBuffer(RenderCore::DynamicRHI* RHI);
		~GBuffer();

		void InitResource(GBufferFlagBits Flag,uint32_t Width,uint32_t Height);
		std::shared_ptr<RenderCore::RHITexture2D> GetDepth() const;
		std::shared_ptr<RenderCore::RHITexture2D> GetSceneColor() const;
		std::shared_ptr<RenderCore::RHITexture2D> GetMotionVector() const;
		std::shared_ptr<RenderCore::RHITexture2D> GetNormalBuffer() const;
	private:
		GBufferPrivate* d_ptr = nullptr;
	};
}