#include "Render/SceneRendering/MeshMaterialRenderCache.h"
#include "GltfModel/GltfMeshBuffer.h"
#include "GltfModel/GltfMesh.h"
#include "Material/GltfMaterial.h"
#include "Material/FurMaterial.h"
#include "Render/PBRMaterialRender.h"
#include "Render/FurMaterialRender.h"
#include "Thread/RenderThread.h"

namespace Engine
{
	std::shared_ptr<MaterialRender> FMeshMaterialRenderCache::GetOrCreate(std::shared_ptr<MeshBase> Mesh, uint64_t StableMaterialRenderCacheKey,
																		   uint64_t SceneMaterialCacheGeneration)
	{
		FMaterialRenderCacheLookupKey Key{};
		Key.StableSlotKey = StableMaterialRenderCacheKey;
		Key.MeshBuffer = reinterpret_cast<uintptr_t>(Mesh->GetMeshBuffer().get());
		Key.Material = reinterpret_cast<uintptr_t>(Mesh->GetMaterial().get());
		Key.DeclaredVtxFeat = Mesh->GetMeshBuffer()->GetDeclaredVertexFeatures();
		Key.SceneGeneration = SceneMaterialCacheGeneration;

		const auto Found = CachedRenders.find(Key);
		if (Found != CachedRenders.end())
			return Found->second;

		std::shared_ptr<PBRMaterialRender> PBRMaterial;
		switch (Mesh->GetMaterial()->GetMaterialType())
		{
		case MaterialBase::MaterialType::PBR:
			PBRMaterial = std::make_shared<PBRMaterialRender>(Mesh->GetMeshBuffer(), Mesh->GetMaterial());
			break;
		case MaterialBase::MaterialType::FUR:
		{
			auto FurMat = std::static_pointer_cast<Engine::FurMaterial>(Mesh->GetMaterial());
			PBRMaterial = std::make_shared<FurMaterialRender>(Mesh->GetMeshBuffer(), FurMat, FurMat->GetFurConfig(), FurMat->GetNoiseTex());
		}
		break;
		default:
			PBRMaterial = std::make_shared<PBRMaterialRender>(Mesh->GetMeshBuffer(), Mesh->GetMaterial());
			break;
		}

		PBRMaterial->InitRenderResource();
		// InitShader enqueues RHICreateVertexShader with a captured raw `this`; drain the render queue before the instance can be destroyed
		// (e.g. BS blend-shape scene → quick switch to Model3) to avoid use-after-free poisoning subsequent draws.
		FlushRenderingCommands();
		CachedRenders.emplace(Key, PBRMaterial);
		return PBRMaterial;
	}
}
