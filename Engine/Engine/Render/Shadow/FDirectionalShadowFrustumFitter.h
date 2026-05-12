#pragma once
#include "Render/MaterialPreFrame.h"
#include "Render/Shadow/ShadowProjectorTypes.h"
#include "core/vec2.h"
#include "math/aabb3.h"
#include "math/matrix4x4.h"
#include "math/vector3.h"
#include <vector>

namespace Engine
{
	struct GltfSceneMeshInfo;

	/** Orthographic directional shadow frustum fitting (no RHI). */
	class FDirectionalShadowFrustumFitter
	{
	public:
		static void SetupDirectionalShadowViewProjection(Light& MainLight, const math::AABB3& SubjectWorldAabb, bool bReceiverRelativeFrustumAdjust,
														 const math::AABB3& ReceiverWorldAabb, const core::vec2i& ShadowMapSize,
														 const FShadowProjectorSceneData& ShadowProjectorScene, bool bExpandOrthoXYFromReceivers,
														 const std::vector<GltfSceneMeshInfo>* SubjectMeshListForFrustum = nullptr,
														 const math::AABB3* SubjectMeshWorldClipAabb = nullptr);
	};
}
