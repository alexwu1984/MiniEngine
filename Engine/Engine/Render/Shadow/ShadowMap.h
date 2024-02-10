#pragma once
#include "core/inc.h"
#include "math/aabb3.h"
#include "math/vector2.h"

namespace Engine
{
	class SceneView;

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

		// Position of the directional light in world space.
		math::Vector3 wsLightPosition;
	};

	class ShadowMap
	{
	public:
		ShadowMap();
		~ShadowMap();

		static void ComputeSceneCascadeParams(std::shared_ptr<SceneView>& sceneView, CascadeParameters& cascadeParams);
	};
}
