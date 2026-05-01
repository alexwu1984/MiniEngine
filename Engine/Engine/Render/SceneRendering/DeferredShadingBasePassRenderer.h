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
	};
}
