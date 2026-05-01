#include "Render/SceneRendering/MeshMaterialRenderCache.h"
#include "GltfModel/GltfMesh.h"
#include "Material/GltfMaterial.h"
#include "Material/GltfFurMaterial.h"
#include "Render/PBRMaterialRender.h"
#include "Render/FurMaterialRender.h"

namespace Engine
{
	std::shared_ptr<MaterialRender> FMeshMaterialRenderCache::GetOrCreate(std::shared_ptr<MeshBase> Mesh)
	{
		const FMaterialRenderCacheKey Key{ Mesh->GetMeshBuffer().get(), Mesh->GetMaterial().get() };
		const auto Found = CachedRenders.find(Key);
		if (Found != CachedRenders.end())
			return Found->second;

		std::shared_ptr<PBRMaterialRender> PBRMaterial;
		switch (Mesh->GetMaterial()->GetMaterialType())
		{
		case GltfMaterial::MaterialType::PBR:
			PBRMaterial = std::make_shared<PBRMaterialRender>(Mesh->GetMeshBuffer(), Mesh->GetMaterial());
			break;
		case GltfMaterial::MaterialType::FUR:
		{
			auto FurMaterial = std::static_pointer_cast<GltfFurMaterial>(Mesh->GetMaterial());
			PBRMaterial = std::make_shared<FurMaterialRender>(Mesh->GetMeshBuffer(), FurMaterial, FurMaterial->GetFurConfig(), FurMaterial->GetNoiseTex());
		}
		break;
		default:
			PBRMaterial = std::make_shared<PBRMaterialRender>(Mesh->GetMeshBuffer(), Mesh->GetMaterial());
			break;
		}

		PBRMaterial->InitRenderResource();
		CachedRenders.emplace(Key, PBRMaterial);
		return PBRMaterial;
	}
}
