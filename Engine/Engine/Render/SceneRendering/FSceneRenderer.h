#pragma once
#include "Render/MaterialPreFrame.h"
#include "Render/SceneRendering/FSceneViewFamily.h"
#include "Render/SceneRendering/FSceneViewData.h"
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

	/**
	 * Per-frame deferred scene renderer (instance).
	 * Game thread: Submit(...) captures immutable frame inputs.
	 * Render thread: Render(RHI) is the single entry that builds the frame graph and records commands.
	 */
	class FSceneRenderer
	{
	public:
		FSceneRenderer() = default;

		/** Record everything needed for one deferred frame (no RHI recording). Safe to call from the game thread. */
		void Submit(SceneRender* SceneRenderSelf, SceneRenderPrivate* ResourceState, const FSceneViewFamily& ViewFamily,
					std::shared_ptr<const FSceneViewData> ViewData, std::vector<GltfSceneMeshInfo> MeshesInfoCopy,
					std::vector<GltfSceneMeshInfo> shadowCasters, std::vector<GltfSceneMeshInfo> shadowFrustumBounds,
					std::vector<Light> ShadowPassLights, std::shared_ptr<Actor> ShadowProjectorActor,
					std::vector<std::shared_ptr<Actor>> AllActorsForShadow);

		/** Execute the submitted frame on the render thread. No-op if Submit was not called or RHI is null. */
		void Render(RenderCore::DynamicRHI* RHI);

	private:
		bool bHasFrame = false;
		SceneRender* SceneRenderSelf = nullptr;
		SceneRenderPrivate* ResourceState = nullptr;
		FSceneViewFamily ViewFamily{};
		std::shared_ptr<const FSceneViewData> ViewData;
		std::vector<GltfSceneMeshInfo> MeshesInfo;
		std::vector<GltfSceneMeshInfo> ShadowCasters;
		std::vector<GltfSceneMeshInfo> ShadowFrustumBounds;
		std::vector<Light> LightsForShadow;
		std::shared_ptr<Actor> ShadowProjectorActor;
		std::vector<std::shared_ptr<Actor>> AllActorsForShadow;
	};
}
