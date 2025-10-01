#pragma once
#include "core/inc.h"

namespace RenderCore
{
	class RHICommandContext;
	class DynamicRHI;
	class RHITexture2D;
	class RHIUnorderedAccessView;
}

namespace Engine
{
	struct GBufferPrivate;

	enum GBufferFlagBits
	{
		GBUFFER_NONE = 0,
		GBUFFER_DEPTH = 0x1,
		GBUFFER_MOTION_VECTORS = 0x3,
		GBUFFER_SCENE_COLOR = 0x7,
		GBUFFER_NORMAL_BUFFER = 0xf,
		GBUFFER_EMISSIVE_BUFFER = 0x1f,
		GBUFFER_METALLIC_ROUGHNESS_BUFFER = 0x3f,
	};

	class GBuffer
	{
	public:
		GBuffer(RenderCore::DynamicRHI* RHI);
		~GBuffer();

		void InitResource(GBufferFlagBits Flag,uint32_t Width,uint32_t Height);
		std::shared_ptr<RenderCore::RHITexture2D> GetDepth() const;
		std::shared_ptr<RenderCore::RHITexture2D> GetSceneColor() const;
		std::shared_ptr<RenderCore::RHITexture2D> GetSceneColorWithSSR() const;
		std::shared_ptr<RenderCore::RHITexture2D> GetSceneColorWithBloom() const;
		std::shared_ptr<RenderCore::RHIUnorderedAccessView> GetSceneColorUAV() const;
		std::shared_ptr<RenderCore::RHITexture2D> GetMotionVector() const;
		std::shared_ptr<RenderCore::RHITexture2D> GetNormalBuffer() const;
		std::shared_ptr<RenderCore::RHITexture2D> GetEmissiveBuffer() const;
		std::shared_ptr<RenderCore::RHITexture2D> GetMetallicRoughnessBuffer() const;
	private:
		GBufferPrivate* d_ptr = nullptr;
	};
}