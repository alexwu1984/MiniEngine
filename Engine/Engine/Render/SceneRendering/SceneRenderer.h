#pragma once
#include "Render/MaterialPreFrame.h"
#include "Render/SceneRendering/SceneViewFamily.h"
#include "Render/SceneRendering/SceneViewData.h"
#include "Render/Shadow/ShadowMap.h"
#include "Scene/GltfMeshComponent.h"
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace RenderCore
{
	class DynamicRHI;
}

namespace Engine
{
	class FWorldSceneRender;
	struct FWorldSceneRenderPrivate;

	/**
	 * Holds one frame's deferred rendering inputs and records them via RDG on the render thread.
	 * FWorldSceneRender is the long-lived owner of scene resources; this object is the per-frame bridge (Submit → Render).
	 */
	class FSceneRenderer
	{
	public:
		FSceneRenderer() = default;

		/** Capture inputs for the next Render call (game thread; no RHI). */
		void Submit(FWorldSceneRender* WorldSceneRenderOwner, FWorldSceneRenderPrivate* SceneResources, const FSceneViewFamily& ViewFamily,
					std::shared_ptr<const FSceneViewData> ViewData, std::vector<GltfSceneMeshInfo> MeshesInfoCopy,
					std::vector<GltfSceneMeshInfo> shadowCasters, std::vector<GltfSceneMeshInfo> shadowFrustumBounds,
					std::vector<Light> ShadowPassLights, FShadowProjectorSceneData ShadowProjectorScene,
					std::optional<std::wstring> SkyLightHdrFullPathOverride);

		/** Execute the last Submit on the render thread. */
		void Render(RenderCore::DynamicRHI* RHI);

	private:
		bool bHasFrame = false;
		FWorldSceneRender* WorldSceneRenderOwner = nullptr;
		FWorldSceneRenderPrivate* SceneResources = nullptr;
		FSceneViewFamily ViewFamily{};
		std::shared_ptr<const FSceneViewData> ViewData;
		std::vector<GltfSceneMeshInfo> MeshesInfo;
		std::vector<GltfSceneMeshInfo> ShadowCasters;
		std::vector<GltfSceneMeshInfo> ShadowFrustumBounds;
		std::vector<Light> LightsForShadow;
		FShadowProjectorSceneData ShadowProjectorScene{};
		std::optional<std::wstring> SkyLightHdrOverrideForFrame{};
	};
} // namespace Engine
