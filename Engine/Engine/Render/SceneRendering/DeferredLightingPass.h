#pragma once
#include "core/inc.h"
#include "Render/MaterialPreFrame.h"
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
	class SceneTextures;
	class FWorldSceneRender;
	struct FSceneViewData;

	/** Fullscreen pass: analytic lights + IBL into scene color from deferred scene textures. */
	class DeferredLightingPass
	{
	public:
		explicit DeferredLightingPass(RenderCore::DynamicRHI* InRHI);

		void InitResource();

		/** RDG pass 1: SceneColor → SceneColorPreLighting (base-pass HDR before lighting). */
		void CopySceneColorToPreLighting(RenderCore::RHICommandContext& RHIContext, const std::shared_ptr<SceneTextures>& TargetBuffer) const;
		/** RDG pass 2: fullscreen deferred lighting into SceneColor (reads PreLighting + scene textures). */
		void ExecuteRaster(RenderCore::RHICommandContext& RHIContext, std::shared_ptr<RenderCore::RHIViewPort> ViewPort, const std::shared_ptr<SceneTextures>& TargetBuffer,
						   FWorldSceneRender* WorldSceneRender, const std::shared_ptr<const FSceneViewData>& ViewData) const;

		/** Copy then raster (single submission path). */
		void Execute(RenderCore::RHICommandContext& RHIContext, std::shared_ptr<RenderCore::RHIViewPort> ViewPort, const std::shared_ptr<SceneTextures>& TargetBuffer,
					 FWorldSceneRender* WorldSceneRender, const std::shared_ptr<const FSceneViewData>& ViewData) const;

		/** Binds IBL + shadow SRVs (t5–t11) and cbPointShadow / cbSpotShadow for fur forward pass (per-frame cbPerFrame comes from FurMaterialRender). */
		void BindFurForwardSharedSRVs(RenderCore::RHICommandContext& RHIContext, const std::shared_ptr<SceneTextures>& TargetBuffer, FWorldSceneRender* WorldSceneRender,
									  const std::shared_ptr<const FSceneViewData>& ViewData) const;

	private:
		RenderCore::DynamicRHI* RHI = nullptr;
		/** Created once in InitResource; avoids per-frame RHICreateUniformBuffer (failures left cb_ null → D3D11 AV). */
		mutable std::unique_ptr<CBPerFrameWrap> PerFrameUniform;
		mutable std::unique_ptr<CBPointShadowWrap> PointShadowUniform;
		mutable std::unique_ptr<CBSpotShadowWrap> SpotShadowUniform;
		std::shared_ptr<RenderCore::RHIVertexShader> VertexShader;
		std::shared_ptr<RenderCore::RHIPixelShader> PixelShader;
		/** Bound to t5/t7 when IBL cubemaps are missing so PS never samples stale 2D PBR textures as cubes. */
		std::shared_ptr<RenderCore::RHITextureCube> FallbackIBLCube;
		/** Bound to t6 when BRDF LUT is missing; matches PF_G32R32F integration LUT layout. */
		std::shared_ptr<RenderCore::RHITexture2D> FallbackBrdfLut;
	};
}
