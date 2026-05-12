#include "Render/Shadow/FShadowSceneBounds.h"
#include "GltfModel/GltfMesh.h"
#include "Material/FurMaterial.h"
#include "Material/MaterialBase.h"
#include "Scene/SceneMeshComponent.h"
#include <algorithm>
#include <cfloat>

namespace Engine
{
	bool FShadowSceneBounds::MeshWritesShadowMapDepth(const std::shared_ptr<MeshBase>& Mesh)
	{
		if (!Mesh)
			return false;
		const auto mat = Mesh->GetMaterial();
		if (!mat)
			return false;
		if (!mat->IsTransparent())
			return true;
		return mat->GetBaseColorTexture() != nullptr;
	}

	static math::AABB3 WorldMeshBoundsForShadowFrustum(const std::shared_ptr<MeshBase>& Mesh, const math::AABB3& TransformedMeshBox)
	{
		if (!Mesh || !Mesh->GetMaterial())
			return TransformedMeshBox;
		auto FurMat = std::dynamic_pointer_cast<FurMaterial>(Mesh->GetMaterial());
		if (!FurMat)
			return TransformedMeshBox;
		const float Reach = (std::max)(0.f, FurMat->GetFurConfig().FurLength) * 1.2f;
		return math::ExpandAabbByMargin(TransformedMeshBox, Reach);
	}

	const std::vector<GltfSceneMeshInfo>* FShadowSceneBounds::SelectShadowSubjectMeshListForFrustum(const std::vector<GltfSceneMeshInfo>& ShadowCasterMeshes,
																									const std::vector<GltfSceneMeshInfo>& FrustumBoundsMeshes,
																									const FShadowProjectorSceneData& ShadowProjectorScene)
	{
		if (kPreferTightShadowFrustumFromCasters)
		{
			if (!ShadowCasterMeshes.empty())
				return &ShadowCasterMeshes;
			if (ShadowProjectorScene.bValid)
				return nullptr;
			if (!FrustumBoundsMeshes.empty())
				return &FrustumBoundsMeshes;
			return &ShadowCasterMeshes;
		}
		return !FrustumBoundsMeshes.empty() ? &FrustumBoundsMeshes : &ShadowCasterMeshes;
	}

	void FShadowSceneBounds::BuildMergedShadowSubjectWorldAabb(const std::vector<GltfSceneMeshInfo>* SubjectMeshList, const FShadowProjectorSceneData& ShadowProjectorScene,
															   math::AABB3& OutSubjectWorldAabb, bool& OutSubjectValid)
	{
		OutSubjectValid = false;
		if (SubjectMeshList)
		{
			for (const auto& MeshInfo : *SubjectMeshList)
			{
				for (const auto& Mesh : MeshInfo.Meshes)
				{
					if (!Mesh || !MeshWritesShadowMapDepth(Mesh))
						continue;
					math::AABB3 wbox = Mesh->GetBoundingBox().Transform(MeshInfo.WorldTransform);
					wbox = WorldMeshBoundsForShadowFrustum(Mesh, wbox);
					OutSubjectWorldAabb = OutSubjectValid ? OutSubjectWorldAabb.MergeAABB(wbox) : wbox;
					OutSubjectValid = true;
				}
			}
		}
		if (!OutSubjectValid && ShadowProjectorScene.bValid)
		{
			OutSubjectWorldAabb = ShadowProjectorScene.ModelLocalAABB.Transform(ShadowProjectorScene.WorldTransform);
			OutSubjectValid = true;
		}
	}

	void FShadowSceneBounds::BuildMergedShadowReceiverWorldAabb(const std::vector<GltfSceneMeshInfo>& FrustumBoundsMeshes, math::AABB3& OutReceiverWorldAabb,
																bool& OutReceiverValid)
	{
		OutReceiverValid = false;
		for (const auto& MeshInfo : FrustumBoundsMeshes)
		{
			for (const auto& Mesh : MeshInfo.Meshes)
			{
				if (!Mesh)
					continue;
				math::AABB3 wbox = Mesh->GetBoundingBox().Transform(MeshInfo.WorldTransform);
				OutReceiverWorldAabb = OutReceiverValid ? OutReceiverWorldAabb.MergeAABB(wbox) : wbox;
				OutReceiverValid = true;
			}
		}
	}

	bool FShadowSceneBounds::TryMergeSubjectMeshesLightSpaceExtents(const std::vector<GltfSceneMeshInfo>* SubjectMeshList, const math::Matrix4x4& LightView,
																	const math::AABB3* OptionalWorldClipAabb, math::Vector3& OutLsMin, math::Vector3& OutLsMax)
	{
		if (!SubjectMeshList || SubjectMeshList->empty())
			return false;
		math::Vector3 lsMin(FLT_MAX, FLT_MAX, FLT_MAX);
		math::Vector3 lsMax(-FLT_MAX, -FLT_MAX, -FLT_MAX);
		bool any = false;
		for (const auto& MeshInfo : *SubjectMeshList)
		{
			for (const auto& Mesh : MeshInfo.Meshes)
			{
				if (!MeshWritesShadowMapDepth(Mesh))
					continue;
				math::AABB3 wbox = WorldMeshBoundsForShadowFrustum(Mesh, Mesh->GetBoundingBox().Transform(MeshInfo.WorldTransform));
				if (OptionalWorldClipAabb)
				{
					math::AABB3 clipVol = *OptionalWorldClipAabb;
					math::AABB3 clipped;
					if (!wbox.GetIntersect(clipVol, clipped))
						continue;
					wbox = clipped;
				}
				math::Vector3 corners[8];
				wbox.GetPoint(corners);
				for (int i = 0; i < 8; ++i)
				{
					const math::Vector3 ls = LightView.TransformPosition(corners[i]);
					lsMin = math::Vector3((std::min)(lsMin.x, ls.x), (std::min)(lsMin.y, ls.y), (std::min)(lsMin.z, ls.z));
					lsMax = math::Vector3((std::max)(lsMax.x, ls.x), (std::max)(lsMax.y, ls.y), (std::max)(lsMax.z, ls.z));
				}
				any = true;
			}
		}
		if (!any)
			return false;
		OutLsMin = lsMin;
		OutLsMax = lsMax;
		return true;
	}
}
