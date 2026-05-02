#include "Render/SceneRendering/OpaqueMeshDrawBuilder.h"
#include "Render/SceneRendering/DeferredBasePassMeshDispatch.h"
#include "Render/SceneRendering/MeshMaterialRenderCache.h"
#include "Render/SceneRendering/TranslucentMeshSorter.h"
#include "Render/SceneRendering/TranslucentMeshSortKey.h"
#include "RHI/DynamicRHI.h"
#include "Scene/SceneMeshComponent.h"
#include "GltfModel/GltfMesh.h"
#include "Material/GltfMaterial.h"

namespace Engine
{
	void FOpaqueMeshDrawBuilder::DrawSortedOpaqueMeshes(RenderCore::DynamicRHI* RHI, const std::vector<GltfSceneMeshInfo>& SceneMeshInfos, const math::Vector3& CameraWorldPos,
													  bool bIsPrePass, const FDeferredBasePassDrawContext& DrawContext, FMeshMaterialRenderCache& MaterialCache)
	{
		for (const auto& SceneMeshInfo : SceneMeshInfos)
		{
			std::vector<FTranslucentMeshSortKey> LocalKeys;
			FTranslucentMeshSorter::AppendPerActorMeshSortKeys(SceneMeshInfo, CameraWorldPos, LocalKeys);
			FTranslucentMeshSorter::SortByDistance(LocalKeys);
			std::stable_sort(LocalKeys.begin(), LocalKeys.end(),
							 [](const FTranslucentMeshSortKey& A, const FTranslucentMeshSortKey& B) {
								 const uint64_t Ma = A.Mesh ? A.Mesh->GetMaterial()->GetStableMaterialInstanceId() : 0;
								 const uint64_t Mb = B.Mesh ? B.Mesh->GetMaterial()->GetStableMaterialInstanceId() : 0;
								 return Ma < Mb;
							 });

			for (const auto& Key : LocalKeys)
			{
				const std::shared_ptr<MeshBase>& Mesh = Key.Mesh;
				std::shared_ptr<MaterialRender> Mat = MaterialCache.GetOrCreate(Mesh, Key.MaterialRenderCacheKey);
				if (!Mesh->GetMaterial()->IsTransparent())
				{
					FDeferredBasePassMeshDispatch::Dispatch(RHI, Mesh, SceneMeshInfo.WorldTransform, SceneMeshInfo.PrevWorldTransform, Mat, bIsPrePass, DrawContext);
				}
			}
		}
	}
}
