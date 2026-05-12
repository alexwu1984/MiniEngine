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
	class RHIComputeShader;
	class RHIViewPort;
	class RHITexture2D;
	class RHITextureCube;
	class RHIStructuredBuffer;
}

namespace Engine
{
	class FSceneTextures;
	class FWorldSceneRender;
	struct FSceneViewData;

	/** Fullscreen pass: analytic lights + IBL into scene color from deferred scene textures. */
	class DeferredLightingPass
	{
	public:
		explicit DeferredLightingPass(RenderCore::DynamicRHI* InRHI);

		void InitResource();

		/** RDG pass 1: SceneColor → SceneColorPreLighting (base-pass HDR before lighting). */
		void CopySceneColorToPreLighting(RenderCore::RHICommandContext& RHIContext, const std::shared_ptr<FSceneTextures>& SceneTextures) const;
		/** RDG pass 2: fullscreen deferred lighting into SceneColor (reads PreLighting + scene textures). */
		void ExecuteRaster(RenderCore::RHICommandContext& RHIContext, std::shared_ptr<RenderCore::RHIViewPort> ViewPort, const std::shared_ptr<FSceneTextures>& SceneTextures,
						   FWorldSceneRender* WorldSceneRender, const std::shared_ptr<const FSceneViewData>& ViewData) const;

		/** Copy then raster (single submission path). */
		void Execute(RenderCore::RHICommandContext& RHIContext, std::shared_ptr<RenderCore::RHIViewPort> ViewPort, const std::shared_ptr<FSceneTextures>& SceneTextures,
					 FWorldSceneRender* WorldSceneRender, const std::shared_ptr<const FSceneViewData>& ViewData) const;

		/** Binds IBL + shadow SRVs (t5–t11) and cbPointShadow / cbSpotShadow / cbDirectionalShadow (b7) for fur/translucent forward (cbPerFrame from material draw). */
		void BindFurForwardSharedSRVs(RenderCore::RHICommandContext& RHIContext, const std::shared_ptr<FSceneTextures>& SceneTextures, FWorldSceneRender* WorldSceneRender,
									  const std::shared_ptr<const FSceneViewData>& ViewData) const;

		/**
		 * Clustered Forward+ pass-1: upload per-view SceneLights (StructuredBuffer), build the cluster CB, and dispatch
		 * `ClusterLightBuildCS` into the UAV cluster table + index list. Called once per frame from the RDG pass that
		 * runs before forward translucent / fur. Idempotent within the same FSceneViewData identity (subsequent calls
		 * skip the upload + dispatch).
		 */
		void DispatchClusterLightCulling(RenderCore::RHICommandContext& RHIContext,
										  const std::shared_ptr<const FSceneViewData>& ViewData) const;

	private:
		/** VS/PS JIT here on first deferred lighting draw so InitResource / ReloadSceneJson flush is not blocked by FXC. */
		void EnsureJitDeferredLightingShaders() const;

		RenderCore::DynamicRHI* RHI = nullptr;
		/** Created once in InitResource; avoids per-frame RHICreateUniformBuffer (failures left cb_ null → D3D11 AV). */
		mutable std::unique_ptr<CBPerFrameWrap> PerFrameUniform;
		mutable std::unique_ptr<CBPointShadowWrap> PointShadowUniform;
		mutable std::unique_ptr<CBSpotShadowWrap> SpotShadowUniform;
		mutable std::unique_ptr<CBDirectionalShadowWrap> DirectionalShadowUniform;
		mutable std::shared_ptr<RenderCore::RHIVertexShader> VertexShader;
		mutable std::shared_ptr<RenderCore::RHIPixelShader> PixelShader;
		/** Bound to t5/t7 when IBL cubemaps are missing so PS never samples stale 2D PBR textures as cubes. */
		std::shared_ptr<RenderCore::RHITextureCube> FallbackIBLCube;
		/** Bound to t6 when BRDF LUT is missing; matches PF_G32R32F integration LUT layout. */
		std::shared_ptr<RenderCore::RHITexture2D> FallbackBrdfLut;

		/**
		 * `StructuredBuffer<Light>` consumed by forward translucent + fur PS at SF_Pixel slot 13. Sized at
		 * kSceneLightBufferCapacity to give clustered Forward+ headroom beyond cbPerFrame.Lights[80]. Filled inside
		 * DispatchClusterLightCulling (once per frame) so the ring slot tracks GPU reads correctly. The CB array
		 * is still filled in parallel because PerFrameStruct helpers (GetMainLight, IsEnableShadow, ...) index
		 * Lights[] directly; PR3 will collapse the two paths.
		 */
		mutable std::shared_ptr<RenderCore::RHIStructuredBuffer> SceneLightBuffer;
		/**
		 * Pointer-identity of the FSceneViewData last consumed by SceneLightBuffer upload / cluster CS dispatch.
		 * Each ExecuteFrame produces a fresh shared FSceneViewData so pointer compare uniquely identifies a frame.
		 * Lets DispatchClusterLightCulling stay idempotent when called more than once per frame.
		 */
		mutable uintptr_t SceneLightLastUploadedViewKey = 0;

		/** RWStructuredBuffer<uint2> output of `ClusterLightBuildCS.hlsl`: per-cluster (offset, count) into the index list. */
		mutable std::shared_ptr<RenderCore::RHIStructuredBuffer> ClusterLightOffsetCountBuffer;
		/** RWStructuredBuffer<uint>  output of `ClusterLightBuildCS.hlsl`: flat list of light indices per cluster. */
		mutable std::shared_ptr<RenderCore::RHIStructuredBuffer> ClusterLightIndexListBuffer;
		mutable std::unique_ptr<CBClusterBuildWrap> ClusterBuildUniform;
		mutable std::shared_ptr<RenderCore::RHIComputeShader> ClusterBuildShader;
		/** Throttles clustered Forward+ CPU split logs; RDG pass timing still records every frame. */
		mutable uint32_t ClusterTimingLogFrameCounter = 0u;
	};
}
