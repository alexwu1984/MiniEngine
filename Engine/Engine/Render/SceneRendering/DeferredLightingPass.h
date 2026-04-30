#pragma once
#include "core/inc.h"
#include <memory>

namespace RenderCore
{
	class DynamicRHI;
	class RHICommandContext;
	class RHIVertexShader;
	class RHIPixelShader;
	class RHIViewPort;
	class RHITexture2D;
	class RHITextureCube;
}

namespace Engine
{
	class GBuffer;
	class SceneRender;
	struct FSceneViewData;

	/** Fullscreen pass: analytic lights + IBL into scene color from G-buffer. */
	class DeferredLightingPass
	{
	public:
		explicit DeferredLightingPass(RenderCore::DynamicRHI* InRHI);

		void InitResource();
		void Execute(RenderCore::RHICommandContext& RHIContext, std::shared_ptr<RenderCore::RHIViewPort> ViewPort, const std::shared_ptr<GBuffer>& TargetBuffer,
					 SceneRender* Scene, const std::shared_ptr<const FSceneViewData>& ViewData) const;

	private:
		RenderCore::DynamicRHI* RHI = nullptr;
		std::shared_ptr<RenderCore::RHIVertexShader> VertexShader;
		std::shared_ptr<RenderCore::RHIPixelShader> PixelShader;
		/** Bound to t5/t7 when IBL cubemaps are missing so PS never samples stale 2D PBR textures as cubes. */
		std::shared_ptr<RenderCore::RHITextureCube> FallbackIBLCube;
		/** Bound to t6 when BRDF LUT is missing; matches PF_G32R32F integration LUT layout. */
		std::shared_ptr<RenderCore::RHITexture2D> FallbackBrdfLut;
	};
}
