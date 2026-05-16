#pragma once
#include "core/inc.h"
#include "RHI/RDGResourceAccess.h"
#include "Render/RDGBuilder.h"
#include "Render/MaterialPreFrame.h"
#include <memory>
#include <vector>

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
	class RHIRenderTarget;
}

namespace Engine
{
	class FSceneTextures;
	class FWorldSceneRender;
	struct FSceneViewData;

	/** IBL/shadow/cluster SRV bindings shared by fur + translucent forward PS. */
	struct FFurForwardSharedSrvSet
	{
		std::shared_ptr<RenderCore::RHITextureCube> IrradianceCube;
		std::shared_ptr<RenderCore::RHITextureCube> SpecularCube;
		std::shared_ptr<RenderCore::RHITexture2D> BrdfLut;
		std::shared_ptr<RenderCore::RHITexture2D> DirectionalShadow;
		std::shared_ptr<RenderCore::RHITextureCube> PointShadowCube;
		std::shared_ptr<RenderCore::RHITexture2D> SpotShadow;
		std::shared_ptr<RenderCore::RHITexture2D> GroundEnvLatLong;
		std::shared_ptr<RenderCore::RHIStructuredBuffer> SceneLights;
		std::shared_ptr<RenderCore::RHIStructuredBuffer> ClusterLightOffsetCount;
		std::shared_ptr<RenderCore::RHIStructuredBuffer> ClusterLightIndexList;
	};

	/** Declares barriers for 2D textures sampled as SRV in BindFurForwardSharedSRVs (IBL LUT/shadow 2D/lat-long). Cubemap slots need AppendFurForwardSharedCubeTextureBarriers. */
	std::vector<FRDGPassResource> GatherFurForwardSharedTwoDimensionalSrvInputs(const FFurForwardSharedSrvSet& S);

	void AppendFurForwardSharedCubeTextureBarriers(std::vector<RenderCore::FRDGTextureBarrierDesc>& Out, const FFurForwardSharedSrvSet& S);
	void AppendFurForwardSharedStructuredBufferPixelSrvBarriers(std::vector<RenderCore::FRDGStructuredBufferBarrierDesc>& Out, const FFurForwardSharedSrvSet& S);

	class DeferredLightingPass
	{
		friend void GatherFurForwardSharedSrvSet(const DeferredLightingPass& Pass, FWorldSceneRender* WorldSceneRender, const FSceneViewData& ViewData,
												 FFurForwardSharedSrvSet& Out);

	public:
		explicit DeferredLightingPass(RenderCore::DynamicRHI* InRHI);

		void InitResource();

		/** Copy SceneColor → SceneColorPreLighting. */
		void CopySceneColorToPreLighting(RenderCore::RHICommandContext& RHIContext, const std::shared_ptr<FSceneTextures>& SceneTextures) const;
		/** Fullscreen deferred lighting into SceneColor. */
		void ExecuteRaster(RenderCore::RHICommandContext& RHIContext, std::shared_ptr<RenderCore::RHIViewPort> ViewPort, const std::shared_ptr<FSceneTextures>& SceneTextures,
						   FWorldSceneRender* WorldSceneRender, const std::shared_ptr<const FSceneViewData>& ViewData) const;

		/** Copy then ExecuteRaster. */
		void Execute(RenderCore::RHICommandContext& RHIContext, std::shared_ptr<RenderCore::RHIViewPort> ViewPort, const std::shared_ptr<FSceneTextures>& SceneTextures,
					 FWorldSceneRender* WorldSceneRender, const std::shared_ptr<const FSceneViewData>& ViewData) const;

		/** Fur/translucent forward: IBL + shadows at t5-t12 and shadow CBs; expects cbPerFrame from material draw. */
		void BindFurForwardSharedSRVs(RenderCore::RHICommandContext& RHIContext, const std::shared_ptr<FSceneTextures>& SceneTextures, FWorldSceneRender* WorldSceneRender,
									  const std::shared_ptr<const FSceneViewData>& ViewData) const;

		/** Per-frame CB + GatherFurForwardSharedSrvSet (call before OM barriers / BindFurForwardSharedSRVs). */
		void PrepareForwardSharedSrvSet(FWorldSceneRender* WorldSceneRender, const std::shared_ptr<const FSceneViewData>& ViewData, FFurForwardSharedSrvSet& OutSrvs) const;

		/** Upload lights, cluster CB, ClusterLightBuildCS; idempotent per FSceneViewData pointer per frame. */
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

		/**
		 * DeferredLighting / fur-forward PS placeholders (`DeferredLighting.hlsl` etc.).
		 * D3D11: t8 ShadowMap, t10 PointShadowCube, t11 spot shadow use SampleCmpLevelZero — only PF_ShadowDepth (2D RT or cube) is valid there.
		 * Do not bind FallbackBrdfLut or FallbackIBLCube on those slots (DEVICE_DRAW_RESOURCE_FORMAT_SAMPLE_C_UNSUPPORTED).
		 */
		/** t5/t7 when sky IBL cubemaps missing. */
		std::shared_ptr<RenderCore::RHITextureCube> FallbackIBLCube;
		/** t6 (and non-compare 2D slots) when BRDF LUT missing. */
		std::shared_ptr<RenderCore::RHITexture2D> FallbackBrdfLut;
		/** 1×1 PF_ShadowDepth, cleared — default for t8 directional map and t11 spot map when real RT unavailable (shared RT is intentional). */
		std::shared_ptr<RenderCore::RHIRenderTarget> FallbackCompareShadowDepthRt;
		/** Small PF_ShadowDepth cube, six faces cleared — default t10 when point cube shadow unavailable. */
		std::shared_ptr<RenderCore::RHITextureCube> FallbackPointShadowCube;

		/** Lights SRV for clustered forward PS (slot 13); filled in DispatchClusterLightCulling. */
		mutable std::shared_ptr<RenderCore::RHIStructuredBuffer> SceneLightBuffer;
		/** Last FSceneViewData pointer used for SceneLight upload + cluster dispatch (idempotency). */
		mutable uintptr_t SceneLightLastUploadedViewKey = 0;

		/** RWStructuredBuffer<uint2> output of `ClusterLightBuildCS.hlsl`: per-cluster (offset, count) into the index list. */
		mutable std::shared_ptr<RenderCore::RHIStructuredBuffer> ClusterLightOffsetCountBuffer;
		/** RWStructuredBuffer<uint>  output of `ClusterLightBuildCS.hlsl`: flat list of light indices per cluster. */
		mutable std::shared_ptr<RenderCore::RHIStructuredBuffer> ClusterLightIndexListBuffer;
		mutable std::unique_ptr<CBClusterBuildWrap> ClusterBuildUniform;
		mutable std::shared_ptr<RenderCore::RHIComputeShader> ClusterBuildShader;
		/** Debug log throttle for cluster timing splits. */
		mutable uint32_t ClusterTimingLogFrameCounter = 0u;
	};
}
