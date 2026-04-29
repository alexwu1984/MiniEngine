#pragma once
#include "Render/SceneRendering/FDeferredBasePassDrawContext.h"
#include <memory>
#include <vector>

namespace RenderCore
{
	class DynamicRHI;
}

namespace Engine
{
	class CameraComponent;
	class FMeshMaterialRenderCache;
	struct GltfSceneMeshInfo;

	/** Issues opaque mesh draws for the deferred base pass in approximate front-to-back order per actor. */
	class FOpaqueMeshDrawBuilder
	{
	public:
		static void DrawSortedOpaqueMeshes(RenderCore::DynamicRHI* RHI, const std::vector<GltfSceneMeshInfo>& SceneMeshInfos, const std::shared_ptr<CameraComponent>& Camera,
										   bool bIsPrePass, const FDeferredBasePassDrawContext& DrawContext, FMeshMaterialRenderCache& MaterialCache);
	};
}
