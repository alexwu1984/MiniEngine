#pragma once
#include "Render/MaterialPreFrame.h"
#include "Render/Shadow/ShadowProjectorTypes.h"
#include "core/vec2.h"
#include "math/aabb3.h"
#include "math/matrix4x4.h"
#include "math/vector3.h"

namespace Engine
{
	/** UE-style: orthographic directional shadow frustum + CSM split math (no RHI). */
	class FDirectionalShadowFrustumFitter
	{
	public:
		static constexpr int kCascadeCount = 3;
		static constexpr float kCSMSplitLambda = 0.82f;

		static void ComputeDirectionalCascadeSplitEnds(float n, float f, float outEnds[kCascadeCount]);

		static math::AABB3 WorldBoundsFromViewProjSliceInverse(const math::Matrix4x4& CameraView, float fovy, float aspectWH, float zn, float zf);

		static void SetupDirectionalShadowViewProjection(Light& MainLight, const math::AABB3& SubjectWorldAabb, bool bReceiverRelativeFrustumAdjust,
														 const math::AABB3& ReceiverWorldAabb, const core::vec2i& ShadowMapSize,
														 const FShadowProjectorSceneData& ShadowProjectorScene, bool bExpandOrthoXYFromReceivers);
	};
}
