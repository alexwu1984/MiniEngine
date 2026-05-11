#pragma once
#include "Render/Shadow/ShadowProjectorTypes.h"
#include "math/aabb3.h"
#include <memory>
#include <vector>

namespace Engine
{
	struct GltfSceneMeshInfo;
	class MeshBase;

	/**
	 * UE-style: world-space shadow subject / receiver bounds from scene meshes and optional projector aggregate.
	 * No RHI, no light matrices — consumed by frustum fitters and depth passes.
	 */
	class FShadowSceneBounds
	{
	public:
		/** When true: fit orthographic shadow XY to shadow casters only; receivers still widen Z (and optional XY). */
		static constexpr bool kPreferTightShadowFrustumFromCasters = true;

		static bool MeshWritesShadowMapDepth(const std::shared_ptr<MeshBase>& Mesh);

		static const std::vector<GltfSceneMeshInfo>* SelectShadowSubjectMeshListForFrustum(const std::vector<GltfSceneMeshInfo>& ShadowCasterMeshes,
																						   const std::vector<GltfSceneMeshInfo>& FrustumBoundsMeshes,
																						   const FShadowProjectorSceneData& ShadowProjectorScene);

		static void BuildMergedShadowSubjectWorldAabb(const std::vector<GltfSceneMeshInfo>* SubjectMeshList, const FShadowProjectorSceneData& ShadowProjectorScene,
													  math::AABB3& OutSubjectWorldAabb, bool& OutSubjectValid);

		static void BuildMergedShadowReceiverWorldAabb(const std::vector<GltfSceneMeshInfo>& FrustumBoundsMeshes, math::AABB3& OutReceiverWorldAabb,
													   bool& OutReceiverValid);
	};
}
