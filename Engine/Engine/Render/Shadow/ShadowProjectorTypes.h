#pragma once
#include "math/aabb3.h"
#include "math/matrix4x4.h"
#include "math/vector3.h"

namespace Engine
{
	/**
	 * Game-thread-only snapshot for shadow directional frustum fitting (render thread reads POD).
	 * `bValid` + ModelLocalAABB: merged ProjShadow bounds from World::BuildShadowProjectorAggregateData().
	 * View bounds: filled when enqueueing the primary view (SubmitSceneForRendering) — intersection with receiver AABB
	 * tightens directional ortho XY under grazing sunlight (same pipeline as ExpandOrthoXY receivers).
	 */
	struct FShadowProjectorSceneData
	{
		bool bValid = false;
		math::Matrix4x4 WorldTransform{};
		math::AABB3 ModelLocalAABB{};
		bool bHasViewWorldBoundsForDirectionalReceiverXY = false;
		math::AABB3 ViewWorldBoundsAabb{};
		math::Matrix4x4 CameraView{};
		math::Vector3 CameraWorldPos{};
		float CameraNearZ = 0.1f;
		float CameraFarZ = 1000.f;
		float CameraFovYRad = 1.f;
		float CameraAspectWH = 1.f;
		/** Unit depth axis in world space — should match primary ViewMatrix depth axis used for receiver heuristics. */
		math::Vector3 CameraForwardWorld{ 0.f, 0.f, 1.f };
	};
}
