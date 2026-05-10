#pragma once
#include "core/inc.h"
#include "math/matrix4x4.h"

namespace RenderCore
{
	class RHICommandContext;
	class DynamicRHI;
	class RHITextureCube;
	class RHITexture2D;
}


namespace Engine
{
	struct SkyLightRenderPassPrivate;

	/** Draws skylight environment cubemap at infinity (fullscreen unproject), UE SkyLight / sky dome analogue. */
	class SkyLightRenderPass
	{
	public:
		SkyLightRenderPass(RenderCore::DynamicRHI* RHI);
		~SkyLightRenderPass();

		void InitResource();
		void Render(RenderCore::RHICommandContext& RHIContext,
					const std::vector<std::shared_ptr<RenderCore::RHITexture2D>>& Targets,
					std::shared_ptr<RenderCore::RHITexture2D> Depth,
					const math::Matrix4x4& SkyInverseViewProj);
		void SetTextureCube(std::shared_ptr<RenderCore::RHITextureCube> TexCube);
	private:
		void InitShader();
	private:
		SkyLightRenderPassPrivate* d_ptr = nullptr;
	};
} // namespace Engine
