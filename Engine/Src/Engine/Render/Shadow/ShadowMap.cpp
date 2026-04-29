#include "Render/Shadow/ShadowMap.h"
#include "Render/MaterialPreFrame.h"
#include "Scene/CameraComponent.h"
#include "Scene/Actor.h"
#include "Scene/GltfMeshComponent.h"

namespace Engine
{
	static constexpr float LIGHT_DISTANCE = 4.0f;

	ShadowMap::ShadowMap()
	{

	}

	ShadowMap::~ShadowMap()
	{

	}

	void ShadowMap::ComputeSceneCascadeParams(const std::vector<Light>& lights, const std::vector<std::shared_ptr<Actor>>& actors, CascadeParameters& cascadeParams)
	{
		math::Vector3 lightDir{ 0.0, 0.0, 1.0f };
		if (!lights.empty())
		{
			lightDir = lights[0].Direction.Normalize();
		}

		cascadeParams.lsNearFar = { std::numeric_limits<float>::lowest(), (std::numeric_limits<float>::max)() };
		cascadeParams.vsNearFar = { std::numeric_limits<float>::lowest(), (std::numeric_limits<float>::max)() };
		cascadeParams.wsShadowCastersVolume = {};
		cascadeParams.wsShadowReceiversVolume = {};
		calculateNearFar(lights, actors, cascadeParams);
	}

	void ShadowMap::Update(const CascadeParameters& cascadesParams)
	{
		lsFar = cascadesParams.lsNearFar.x;
		lsNear = cascadesParams.lsNearFar.y;
	}

	static float RayPlaneIntersect(math::Vector3 rayOrigin, math::Vector3 rayDirection, math::Vector3 planeOrigin, math::Vector3 planeNormal)
	{
		float dist = planeNormal.Dot(planeOrigin - rayOrigin) / planeNormal.Dot(rayDirection);
		return dist;
	}

	void ShadowMap::calculateNearFar(const std::vector<Light>& lights, const std::vector<std::shared_ptr<Actor>>& actors, CascadeParameters& cascadeParams)
	{
		std::shared_ptr<Actor> projActor;
		for (const auto& actor : actors)
		{
			if (actor && actor->IsProjectShadow())
			{
				projActor = actor;
				break;
			}
		}
		if (!projActor)
		{
			return;
		}

		math::Matrix4x4 toWsMat = projActor->GetWorldTransform();
		auto modelBox = projActor->GetComponent<GltfMeshComponent>()->GetModelBox();

		math::Vector3 wsSceneCorners[8]{};
		modelBox.GetPoint(wsSceneCorners);
		math::Vector3 lightDir{ 0.0, 0.0, 1.0f };
		if (!lights.empty())
		{
			lightDir = lights[0].Direction.Normalize();
		}

		math::Vector3 lightPos = math::Vector3() + ((lightDir*-1) * LIGHT_DISTANCE);
		float disMin = FLT_MAX, disMax = -FLT_MAX;
		for (size_t i = 0; i < _ARRAYSIZE(wsSceneCorners); i++)
		{
			math::Vector3 temp = toWsMat.TransformPosition(wsSceneCorners[i]);
			float d = RayPlaneIntersect(temp, (lightDir * -1), lightPos, lightDir);

			disMin = (std::min)(disMin, d);
			disMax = (std::max)(disMax, d);
		}

		cascadeParams.lsNearFar.y = disMin;
		cascadeParams.lsNearFar.x = disMax;
	}

	math::Vector2 ShadowMap::computeNearFar(const math::Matrix4x4 view, const math::AABB3& wsShadowCastersVolume)
	{
		math::Vector3 corners[8]{};
		wsShadowCastersVolume.GetPoint(corners);
		return computeNearFar(view, corners, 8);
	}

	math::Vector2 ShadowMap::computeNearFar(const math::Matrix4x4 view, const math::Vector3* wsVertices, size_t count)
	{
		math::Vector2 nearFar = { std::numeric_limits<float>::lowest(), (std::numeric_limits<float>::max)() };
		for (size_t i = 0; i < count; i++)
		{
			math::Vector3 tmp = view.TranslateVectorWithPrespDiv(wsVertices[i]);
			nearFar.x = (std::max)(nearFar.x, tmp.z);  // near
			nearFar.y = (std::min)(nearFar.y, tmp.z);  // far
		}

		return nearFar;
	}

}