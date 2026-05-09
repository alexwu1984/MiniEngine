#pragma once
#include "Render/SceneRendering/DeferredBasePassDrawContext.h"
#include <vector>

namespace RenderCore
{
	class DynamicRHI;
}

namespace Engine
{
	class FMeshMaterialRenderCache;
	struct GltfSceneMeshInfo;
	class DeferredLightingPass;

	/** Records opaque and translucent mesh draws into the deferred scene texture targets. */
	class FDeferredShadingBasePassRenderer
	{
	public:
		static void RenderBasePassOpaque(RenderCore::DynamicRHI* RHI, const std::vector<GltfSceneMeshInfo>& SceneMeshInfos, const FDeferredBasePassDrawContext& DrawContext,
										 FMeshMaterialRenderCache& MaterialCache);
		static void RenderBasePassTranslucent(RenderCore::DynamicRHI* RHI, const std::vector<GltfSceneMeshInfo>& SceneMeshInfos, const FDeferredBasePassDrawContext& DrawContext,
											  FMeshMaterialRenderCache& MaterialCache);
		static void RenderDeferredBasePassFullSequence(RenderCore::DynamicRHI* RHI, const std::vector<GltfSceneMeshInfo>& SceneMeshInfos, const FDeferredBasePassDrawContext& DrawContext,
													   FMeshMaterialRenderCache& MaterialCache);

		/** Forward translucent PBR onto lit SceneColor (after deferred lighting). */
		static void RenderTranslucentForwardAfterDeferredLighting(RenderCore::DynamicRHI* RHI, const std::vector<GltfSceneMeshInfo>& SceneMeshInfos,
																  const FDeferredBasePassDrawContext& DrawContext, FMeshMaterialRenderCache& MaterialCache,
																  DeferredLightingPass* DeferredLighting);

		/** Forward fur shells onto lit SceneColor (after deferred lighting). */
		static void RenderFurForwardAfterDeferredLighting(RenderCore::DynamicRHI* RHI, const std::vector<GltfSceneMeshInfo>& SceneMeshInfos, const FDeferredBasePassDrawContext& DrawContext,
														  FMeshMaterialRenderCache& MaterialCache, DeferredLightingPass* DeferredLighting);
	};
}
