#pragma once
#include <vector>

namespace Engine
{
	class SceneView;
	struct GltfSceneMeshInfo;

	/** One-frame primitive lists produced for the active scene view (visible draws + shadow pass inputs). */
	struct FPrimitiveGatherResult
	{
		std::vector<GltfSceneMeshInfo> VisiblePrimitives;
		std::vector<GltfSceneMeshInfo> DynamicShadowCastingPrimitives;
		std::vector<GltfSceneMeshInfo> ShadowFrustumCullPrimitives;
	};

	/** Single-pass gather of visible mesh primitives and shadow-related subsets for the view. */
	class FSceneRendererPrimitiveGather
	{
	public:
		static void GatherVisiblePrimitives(const SceneView& View, FPrimitiveGatherResult& OutResult);
	};
}
