#include "Render/Shadow/FShadowSceneBounds.h"
#include "GltfModel/GltfMesh.h"
#include "Material/FurMaterial.h"
#include "Material/MaterialBase.h"
#include "Scene/SceneMeshComponent.h"

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
}
