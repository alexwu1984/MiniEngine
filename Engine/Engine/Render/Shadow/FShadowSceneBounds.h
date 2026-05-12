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

		/**
		 * glTFSample-style (GetAABBInGivenSpace + merge): for each shadow-writing mesh, world AABB → 8 corners in light space, merge min/max.
		 * When OptionalWorldClipAabb is set (e.g. CSM cascade hull), each mesh box is intersected with it first so the frustum matches visible casters in that slice.
		 */
		static bool TryMergeSubjectMeshesLightSpaceExtents(const std::vector<GltfSceneMeshInfo>* SubjectMeshList, const math::Matrix4x4& LightView,
														  const math::AABB3* OptionalWorldClipAabb, math::Vector3& OutLsMin, math::Vector3& OutLsMax);
	};
}
