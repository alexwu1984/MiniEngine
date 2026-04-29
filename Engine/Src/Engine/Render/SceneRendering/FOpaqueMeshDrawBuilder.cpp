#include "Render/SceneRendering/FOpaqueMeshDrawBuilder.h"
#include "Render/SceneRendering/FDeferredBasePassMeshDispatch.h"
#include "Render/SceneRendering/FMeshMaterialRenderCache.h"
#include "Render/SceneRendering/FTranslucentMeshSorter.h"
#include "Render/SceneRendering/FTranslucentMeshSortKey.h"
#include "RHI/DynamicRHI.h"
#include "Scene/CameraComponent.h"
#include "Scene/GltfMeshComponent.h"
#include "GltfModel/GltfMesh.h"
#include "Material/GltfMaterial.h"

namespace Engine
{
	void FOpaqueMeshDrawBuilder::DrawSortedOpaqueMeshes(RenderCore::DynamicRHI* RHI, const std::vector<GltfSceneMeshInfo>& SceneMeshInfos, const std::shared_ptr<CameraComponent>& Camera,
													  bool bIsPrePass, const FDeferredBasePassDrawContext& DrawContext, FMeshMaterialRenderCache& MaterialCache)
	{
		for (const auto& SceneMeshInfo : SceneMeshInfos)
		{
			std::vector<FTranslucentMeshSortKey> LocalKeys;
			FTranslucentMeshSorter::AppendPerActorMeshSortKeys(SceneMeshInfo, Camera->GetCameraPos(), LocalKeys);
			FTranslucentMeshSorter::SortByDistance(LocalKeys);
			std::stable_sort(LocalKeys.begin(), LocalKeys.end(),
							 [](const FTranslucentMeshSortKey& A, const FTranslucentMeshSortKey& B) {
								 return A.Mesh->GetMaterial().get() < B.Mesh->GetMaterial().get();
							 });

			for (const auto& Key : LocalKeys)
			{
				const std::shared_ptr<MeshBase>& Mesh = Key.Mesh;
				std::shared_ptr<MaterialRender> Mat = MaterialCache.GetOrCreate(Mesh);
				if (!Mesh->GetMaterial()->IsTransparent())
				{
					FDeferredBasePassMeshDispatch::Dispatch(RHI, Mesh, SceneMeshInfo.WorldTransform, SceneMeshInfo.PrevWorldTransform, Mat, Camera, bIsPrePass, DrawContext);
				}
			}
		}
	}
}
