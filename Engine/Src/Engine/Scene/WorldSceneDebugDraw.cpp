#include "Scene/WorldSceneDebugDraw.h"
#include "Render/MaterialPreFrame.h"
#include "Render/Shadow/ShadowRenderPass.h"
#include "Scene/Actor.h"
#include "Scene/DirectionalLightComponent.h"
#include "Scene/PointLightComponent.h"
#include "Scene/SpotLightComponent.h"
#include "Scene/World.h"
#include <algorithm>

namespace Engine
{
	namespace
	{
		int IndexOfFirstLightOfType(const std::vector<Light>& lights, int type)
		{
			for (int li = 0; li < static_cast<int>(lights.size()); ++li)
			{
				if (lights[static_cast<size_t>(li)].Type == type)
					return li;
			}
			return -1;
		}
	} // namespace

	void WorldSceneDebugDraw::SetShowSceneMeshBoundsDebug(bool bIn)
	{
		std::lock_guard<std::mutex> lock(mutex_);
		bShowSceneMeshBoundsDebug = bIn;
	}

	bool WorldSceneDebugDraw::GetShowSceneMeshBoundsDebug() const
	{
		std::lock_guard<std::mutex> lock(mutex_);
		return bShowSceneMeshBoundsDebug;
	}

	void WorldSceneDebugDraw::SetShowShadowCasterMeshBoundsDebug(bool bIn)
	{
		std::lock_guard<std::mutex> lock(mutex_);
		bShowShadowCasterMeshBoundsDebug = bIn;
	}

	bool WorldSceneDebugDraw::GetShowShadowCasterMeshBoundsDebug() const
	{
		std::lock_guard<std::mutex> lock(mutex_);
		return bShowShadowCasterMeshBoundsDebug;
	}

	void WorldSceneDebugDraw::SetShowDirectionalCSMCascadeSubjectBoundsDebug(bool bIn)
	{
		std::lock_guard<std::mutex> lock(mutex_);
		bShowDirectionalCSMCascadeSubjectBoundsDebug = bIn;
		if (!bIn)
			dirCascadeSubjectAabbDebugCount = 0;
	}

	bool WorldSceneDebugDraw::GetShowDirectionalCSMCascadeSubjectBoundsDebug() const
	{
		std::lock_guard<std::mutex> lock(mutex_);
		return bShowDirectionalCSMCascadeSubjectBoundsDebug;
	}

	void WorldSceneDebugDraw::GetDirectionalCSMCascadeSubjectDebugCopy(int& OutCount, math::AABB3 OutBoxes[3]) const
	{
		std::lock_guard<std::mutex> lock(mutex_);
		OutCount = (std::clamp)(dirCascadeSubjectAabbDebugCount, 0, 3);
		for (int i = 0; i < 3; ++i)
			OutBoxes[i] = dirCascadeSubjectWorldAabbDebug[i];
	}

	void WorldSceneDebugDraw::UpdateDirectionalCSMCascadeSubjectDebugFromShadowPass(const ShadowRenderPass* P)
	{
		std::lock_guard<std::mutex> lock(mutex_);
		if (!bShowDirectionalCSMCascadeSubjectBoundsDebug)
		{
			dirCascadeSubjectAabbDebugCount = 0;
			return;
		}
		if (!P)
		{
			dirCascadeSubjectAabbDebugCount = 0;
			return;
		}
		const CBDirectionalShadow& cb = P->GetCachedDirectionalShadow();
		if (cb.DirectionalCSMEnabled != 1)
		{
			dirCascadeSubjectAabbDebugCount = 0;
			return;
		}
		P->GetDirectionalCSMCascadeSubjectAABBs(dirCascadeSubjectAabbDebugCount, dirCascadeSubjectWorldAabbDebug);
	}

	void WorldSceneDebugDraw::ResetDirectionalCascadeSubjectOverlay(bool bClearShowFlag)
	{
		std::lock_guard<std::mutex> lock(mutex_);
		if (bClearShowFlag)
			bShowDirectionalCSMCascadeSubjectBoundsDebug = false;
		dirCascadeSubjectAabbDebugCount = 0;
	}

	void WorldSceneDebugDraw::CollectShadowDebugLightShapes(const World& WorldRef, ShadowRenderPass* ShadowPass,
															const std::vector<Light>& ShadowPassLights, FShadowDebugWireSubmit& OutSubmit) const
	{
		if (!ShadowPass)
			return;

		// Directional arrow
		{
			Light Dir{};
			int dirLi = -1;
			if (ShadowPass->TryGetCachedMainLightForShading(Dir, &dirLi) && dirLi >= 0)
			{
				const int firstD = IndexOfFirstLightOfType(ShadowPassLights, LightType_Directional);
				if (firstD >= 0 && dirLi >= firstD)
				{
					const int subDir = dirLi - firstD;
					const auto dirComps = WorldRef.GetDirectionalLightsForEditingSorted();
					if (subDir >= 0 && subDir < static_cast<int>(dirComps.size()) && dirComps[static_cast<size_t>(subDir)]
						&& dirComps[static_cast<size_t>(subDir)]->GetShowShadowFrustumDebug()
						&& OutSubmit.NumDir < FShadowDebugWireSubmit::kMaxDebugLights)
					{
						const auto comp = dirComps[static_cast<size_t>(subDir)];
						const auto owner = comp ? comp->GetOwner() : nullptr;
						FShadowDebugWireSubmit::FDirArrow a{};
						a.Origin = owner ? owner->GetPosition() : math::Vector3(0.f, 0.f, 0.f);
						a.DirectionTowardSource = comp ? comp->GetWorldDirection() : math::Vector3(0.f, 1.f, 0.f);
						a.Length = 2.5f;
						OutSubmit.Dir[OutSubmit.NumDir++] = a;
					}
				}
			}
		}

		// Spot cone
		{
			int spotIdx = -1;
			math::Matrix4x4 spotVp{};
			math::Matrix4x4 spotView{};
			if (ShadowPass->TryGetCachedSpotShadowForDeferred(spotIdx, spotVp, &spotView) && spotIdx >= 0)
			{
				const int firstS = IndexOfFirstLightOfType(ShadowPassLights, LightType_Spot);
				if (firstS >= 0 && spotIdx >= firstS)
				{
					const int subS = spotIdx - firstS;
					const auto spots = WorldRef.GetSpotLightsForEditingSorted();
					if (subS >= 0 && subS < static_cast<int>(spots.size()) && spots[static_cast<size_t>(subS)]
						&& spots[static_cast<size_t>(subS)]->GetShowShadowFrustumDebug()
						&& OutSubmit.NumSpot < FShadowDebugWireSubmit::kMaxDebugLights)
					{
						const auto comp = spots[static_cast<size_t>(subS)];
						const auto owner = comp ? comp->GetOwner() : nullptr;
						FShadowDebugWireSubmit::FSpotCone c{};
						c.Apex = owner ? owner->GetPosition() : math::Vector3(0.f, 0.f, 0.f);
						c.ConeAxis = comp ? comp->GetConeAxisWorld() : math::Vector3(0.f, 0.f, 1.f);
						c.Range = comp ? comp->GetRange() : 10.f;
						c.OuterConeCos = comp ? comp->GetOuterConeCos() : 0.70710677f;
						OutSubmit.Spot[OutSubmit.NumSpot++] = c;
					}
				}
			}
		}

		// Point sphere
		{
			int pointIdx = -1;
			math::Matrix4x4 pointFaceVp[6]{};
			math::Vector4 pointPosRange{};
			if (ShadowPass->TryGetCachedPointShadowForDeferred(pointIdx, pointFaceVp, pointPosRange) && pointIdx >= 0)
			{
				const int firstP = IndexOfFirstLightOfType(ShadowPassLights, LightType_Point);
				if (firstP >= 0 && pointIdx >= firstP)
				{
					const int subP = pointIdx - firstP;
					const auto points = WorldRef.GetPointLightsForEditingSorted();
					if (subP >= 0 && subP < static_cast<int>(points.size()) && points[static_cast<size_t>(subP)]
						&& points[static_cast<size_t>(subP)]->GetShowShadowFrustumDebug()
						&& OutSubmit.NumPoint < FShadowDebugWireSubmit::kMaxDebugLights)
					{
						FShadowDebugWireSubmit::FPointSphere s{};
						s.Center = math::Vector3(pointPosRange.x, pointPosRange.y, pointPosRange.z);
						s.Radius = pointPosRange.w;
						OutSubmit.Point[OutSubmit.NumPoint++] = s;
					}
				}
			}
		}
	}
}
