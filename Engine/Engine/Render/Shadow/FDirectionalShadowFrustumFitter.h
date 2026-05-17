#pragma once
#include "Render/MaterialPreFrame.h"
#include "Render/Shadow/ShadowProjectorTypes.h"
#include "core/vec2.h"
#include "math/aabb3.h"
#include "math/matrix4x4.h"
#include "math/vector3.h"

namespace Engine
{
	struct GltfSceneMeshInfo;

	/** Bundles inputs for `SetupDirectionalShadowViewProjection` (ortho fit + optional receiver XY expand). */
	struct FDirectionalShadowFrustumFitParams
	{
		const math::AABB3& SubjectWorldAabb;
		const math::AABB3& ReceiverWorldAabb;
		core::vec2i ShadowMapSize{};
		const FShadowProjectorSceneData& ShadowProjectorScene;
		bool bReceiverRelativeFrustumAdjust = false;
		bool bExpandOrthoXYFromReceivers = false;
		const std::vector<GltfSceneMeshInfo>* SubjectMeshListForFrustum = nullptr;
		const math::AABB3* SubjectMeshWorldClipAabb = nullptr;
	};

	/** Inputs for `ComputeCascadeSubjectWorldAabb` (CSM slice ∩ caster subject in world space). */
	struct FCascadeSubjectWorldAabbParams
	{
		const FShadowProjectorSceneData& Scene;
		const math::AABB3& CasterSubjectWorld;
		const math::AABB3* ReceiverWorld = nullptr;
		int CascadeIndex = 0;
		float ZeSplitBoundaries[2]{};
		int CascadeCount = 1;
	};

	/** Orthographic directional shadow frustum fitting (no RHI). */
	class FDirectionalShadowFrustumFitter
	{
	public:
		/** Max cascades for directional CSM (vertical atlas tiles). */
		static constexpr int kMaxDirectionalCascades = 3;
		/** Normalized split t in (0,1) along [CameraNearZ, CameraFarZ]; small min so first split can sit within a few meters when Far is large (e.g. 1000). */
		static constexpr float kCascadeSplitNormMin = 0.001f;
		static constexpr float kCascadeSplitNormMax = 0.999f;
		/** For 3 cascades: enforce Split1 - Split0 >= this after clamp. */
		static constexpr float kCascadeSplitPairMinGap = 0.001f;

		/** Filament-style: splitZE[i] = world-space ze boundary between cascade i and i+1 (ze = dot(world-cam, view forward)). */
		static void FillLinearZeSplitEnds(float nearZ, float farZ, int cascadeCount, float split0Norm, float split1Norm, float outSplitZE[2]);

		static math::AABB3 ComputeCascadeSubjectWorldAabb(const FCascadeSubjectWorldAabbParams& Params);

		static void SetupDirectionalShadowViewProjection(Light& MainLight, const FDirectionalShadowFrustumFitParams& Params);
	};
}
