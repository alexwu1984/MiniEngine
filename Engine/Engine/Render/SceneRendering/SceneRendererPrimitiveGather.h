#pragma once
#include "Scene/SceneMeshComponent.h"
#include <memory>
#include <vector>

namespace Engine
{
	struct FSceneViewData;
	class FScene;

	/** One-frame primitive lists produced for the active scene view (visible draws + shadow pass inputs). */
	struct FPrimitiveGatherResult
	{
		std::vector<GltfSceneMeshInfo> VisiblePrimitives;
		std::vector<GltfSceneMeshInfo> DynamicShadowCastingPrimitives;
		std::vector<GltfSceneMeshInfo> ShadowFrustumCullPrimitives;
	};

	/** Single-pass gather from FScene primitive proxies (UE-style) + view culling. */
	class FSceneRendererPrimitiveGather
	{
	public:
		static void GatherVisiblePrimitives(const FSceneViewData& ViewData, const FScene& Scene, FPrimitiveGatherResult& OutResult);
	};
}
