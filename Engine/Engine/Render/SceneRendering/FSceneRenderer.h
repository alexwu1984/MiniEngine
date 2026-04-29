#pragma once
#include "Render/MaterialPreFrame.h"
#include "Render/SceneRendering/FSceneViewFamily.h"
#include "Scene/GltfMeshComponent.h"
#include <memory>
#include <vector>

namespace RenderCore
{
	class DynamicRHI;
}

namespace Engine
{
	class Actor;
	class SceneRender;
	struct SceneRenderPrivate;

	/** Per-frame scene renderer: records RDG + passes from immutable view data (UE FSceneRenderer analogue). */
	class FSceneRenderer
	{
	public:
		static void ExecuteDeferredFrame(RenderCore::DynamicRHI* RHI, SceneRender* SceneRenderSelf, SceneRenderPrivate* ResourceState, const FSceneViewFamily& ViewFamily,
										 std::shared_ptr<const FSceneViewData> ViewData, std::vector<GltfSceneMeshInfo> MeshesInfoCopy, std::vector<GltfSceneMeshInfo> shadowCasters,
										 std::vector<GltfSceneMeshInfo> shadowFrustumBounds, std::vector<Light> ShadowPassLights, std::shared_ptr<Actor> ShadowProjectorActor,
										 std::vector<std::shared_ptr<Actor>> AllActorsForShadow);
	};
}
