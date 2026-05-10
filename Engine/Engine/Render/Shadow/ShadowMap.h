#pragma once
#include "core/inc.h"
#include "math/aabb3.h"
#include "math/vector2.h"
#include "math/vector3.h"
#include "math/matrix4x4.h"
#include "Render/MaterialPreFrame.h"
#include <vector>

namespace Engine
{
	/**
	 * Game-thread-only snapshot for shadow cascade / directional fitting (UE-style: render thread reads POD).
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
		/** When true, directional shadows use cascaded splits derived from the primary camera (SubmitSceneForRendering). */
		bool bHasCascadeCameraParams = false;
		math::Matrix4x4 CameraView{};
		math::Vector3 CameraWorldPos{};
		float CameraNearZ = 0.1f;
		float CameraFarZ = 1000.f;
		float CameraFovYRad = 1.f;
		float CameraAspectWH = 1.f;
		/** Unit depth axis in world space — must match primary ViewMatrix column used for view-space Z (CSM split = dot(worldPos - cam, this)). */
		math::Vector3 CameraForwardWorld{ 0.f, 0.f, 1.f };
	};

	struct CascadeParameters 
	{
		// The near and far planes, in clip space, to use for this shadow map
		math::Vector2 csNearFar = { -1.0f, 1.0f };

		// The following fields are set by computeSceneCascadeParams.

		// Light-space near/far planes for the scene.
		math::Vector2 lsNearFar;

		// View-space near/far planes for the scene.
		math::Vector2 vsNearFar;
		math::AABB3 wsShadowCastersVolume;
		math::AABB3 wsShadowReceiversVolume;
	};

	class ShadowMap
	{
	public:
		ShadowMap();
		~ShadowMap();

		static void ComputeSceneCascadeParams(const std::vector<Light>& lights, const FShadowProjectorSceneData& ProjectorScene, CascadeParameters& cascadeParams);
		void Update(const CascadeParameters& cascadesParams);
	private:
		static void calculateNearFar(const std::vector<Light>& lights, const FShadowProjectorSceneData& ProjectorScene, CascadeParameters& cascadeParams);
		static math::Vector2 computeNearFar(const math::Matrix4x4 view, const math::AABB3& wsShadowCastersVolume) ;
		static math::Vector2 computeNearFar(const math::Matrix4x4 view, const math::Vector3* wsVertices, size_t count) ;
	public:
		float lsNear = 0.f;
		float lsFar = 0.f;
	};
}
