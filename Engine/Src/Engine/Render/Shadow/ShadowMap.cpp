#include "Render/Shadow/ShadowMap.h"
#include "Render/MaterialPreFrame.h"
#include "Scene/CameraComponent.h"

namespace Engine
{
	// Directional "light position" used only for projecting near/far along the light direction.
	// Keep it scale-aware; a fixed small distance breaks when the projector scene is large.
	static constexpr float kMinDirectionalLightDistance = 4.0f;

	static math::Vector3 ShadowFirstDirectionOrDefault(const std::vector<Light>& lights)
	{
		for (const Light& L : lights)
		{
			if (L.Type == LightType_Directional)
			{
				math::Vector3 d = L.Direction.Normalize();
				return d.GetSqrLength() < 1e-12f ? math::Vector3{ 0.f, 0.f, 1.f } : d;
			}
		}
		return math::Vector3{ 0.f, 0.f, 1.f };
	}

	ShadowMap::ShadowMap()
	{

	}

	ShadowMap::~ShadowMap()
	{

	}

	void ShadowMap::ComputeSceneCascadeParams(const std::vector<Light>& lights, const FShadowProjectorSceneData& ProjectorScene, CascadeParameters& cascadeParams)
	{
		cascadeParams.lsNearFar = { std::numeric_limits<float>::lowest(), (std::numeric_limits<float>::max)() };
		cascadeParams.vsNearFar = { std::numeric_limits<float>::lowest(), (std::numeric_limits<float>::max)() };
		cascadeParams.wsShadowCastersVolume = {};
		cascadeParams.wsShadowReceiversVolume = {};
		calculateNearFar(lights, ProjectorScene, cascadeParams);
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

	void ShadowMap::calculateNearFar(const std::vector<Light>& lights, const FShadowProjectorSceneData& ProjectorScene, CascadeParameters& cascadeParams)
	{
		if (!ProjectorScene.bValid)
			return;

		const math::Matrix4x4& toWsMat = ProjectorScene.WorldTransform;
		const math::AABB3& modelBox = ProjectorScene.ModelLocalAABB;

		math::Vector3 wsSceneCorners[8]{};
		modelBox.GetPoint(wsSceneCorners);
		const math::Vector3 lightDir = ShadowFirstDirectionOrDefault(lights);

		const math::AABB3 wsAabb = modelBox.Transform(toWsMat);
		const float subjectRadius = (std::max)(wsAabb.GetRadius(), 0.01f);
		const float lightDistance = (std::max)(kMinDirectionalLightDistance, subjectRadius * 2.0f);
		const math::Vector3 lightPos = wsAabb.GetCenter() + (lightDir * lightDistance);
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
