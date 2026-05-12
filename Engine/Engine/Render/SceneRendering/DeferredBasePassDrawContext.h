#pragma once
#include "core/inc.h"

namespace RenderCore
{
	class DynamicRHI;
	class RHICommandContext;
}

namespace Engine
{
	class DeferredLightingPass;
	class FMeshMaterialRenderCache;
	class FSceneTextures;
	class FWorldSceneRender;
	struct FSceneViewData;
	struct GltfSceneMeshInfo;

	/** View and scene bindings consumed while recording deferred base pass draws. */
	struct FDeferredBasePassDrawContext
	{
		RenderCore::DynamicRHI* RHI = nullptr;
		std::shared_ptr<const FSceneViewData> ViewData;
		std::shared_ptr<FSceneTextures> SceneTextures;
		FWorldSceneRender* WorldSceneRender = nullptr;
		/** Command list used for the entire frame (D3D12 requires a single consistent recording context per submission). */
		RenderCore::RHICommandContext* RHICmdList = nullptr;
		std::shared_ptr<std::vector<GltfSceneMeshInfo>> MeshesForDraw;
		FMeshMaterialRenderCache* MaterialCache = nullptr;
		DeferredLightingPass* DeferredLighting = nullptr;
	};
}
