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
	struct SceneTexturesPrivate;

	enum class ESceneTexturesFlags : uint32_t
	{
		None = 0,
		SceneDepth = 0x1,
		SceneVelocity = 0x3,
		SceneColor = 0x7,
		DeferredNormals = 0xf,
		DeferredEmissive = 0x1f,
		DeferredMetallicRoughness = 0x3f,
		DeferredMaterialAux = 0x40,
	};

	class SceneTextures
	{
	public:
		SceneTextures(RenderCore::DynamicRHI* RHI);
		~SceneTextures();

		void InitResource(ESceneTexturesFlags Flags, uint32_t Width, uint32_t Height);
		void InitDefaultSceneTargets(uint32_t Width, uint32_t Height);
		std::shared_ptr<RenderCore::RHITexture2D> GetDepth() const;
		std::shared_ptr<RenderCore::RHITexture2D> GetSceneColor() const;
		std::shared_ptr<RenderCore::RHITexture2D> GetSceneColorWithSSR() const;
		std::shared_ptr<RenderCore::RHITexture2D> GetSceneColorWithBloom() const;
		std::shared_ptr<RenderCore::RHIUnorderedAccessView> GetSceneColorUAV() const;
		std::shared_ptr<RenderCore::RHITexture2D> GetMotionVector() const;
		std::shared_ptr<RenderCore::RHITexture2D> GetNormalBuffer() const;
		std::shared_ptr<RenderCore::RHITexture2D> GetEmissiveBuffer() const;
		std::shared_ptr<RenderCore::RHITexture2D> GetMetallicRoughnessBuffer() const;
		std::shared_ptr<RenderCore::RHITexture2D> GetMaterialAuxBuffer() const;
		std::shared_ptr<RenderCore::RHITexture2D> GetSceneColorPreLighting() const;

	private:
		SceneTexturesPrivate* d_ptr = nullptr;
	};
}
