#include "Render/SceneRendering/FDeferredShadingBasePassRenderer.h"
#include "Render/SceneRendering/FOpaqueMeshDrawBuilder.h"
#include "Render/SceneRendering/FDeferredBasePassMeshDispatch.h"
#include "Render/SceneRendering/FMeshMaterialRenderCache.h"
#include "Render/SceneRendering/FTranslucentMeshSorter.h"
#include "Render/SceneRendering/FTranslucentMeshSortKey.h"
#include "Render/SceneRendering/FSceneViewData.h"
#include "RHI/DynamicRHI.h"
#include "Scene/GltfMeshComponent.h"
#include "GltfModel/GltfMesh.h"
#include "Material/GltfMaterial.h"

namespace Engine
{
	void FDeferredShadingBasePassRenderer::RenderBasePassOpaque(RenderCore::DynamicRHI* RHI, const std::vector<GltfSceneMeshInfo>& SceneMeshInfos, const FDeferredBasePassDrawContext& DrawContext,
															  FMeshMaterialRenderCache& MaterialCache)
	{
		const math::Vector3 CamPos = DrawContext.ViewData ? DrawContext.ViewData->CameraPos : math::Vector3();
		FOpaqueMeshDrawBuilder::DrawSortedOpaqueMeshes(RHI, SceneMeshInfos, CamPos, true, DrawContext, MaterialCache);
		FOpaqueMeshDrawBuilder::DrawSortedOpaqueMeshes(RHI, SceneMeshInfos, CamPos, false, DrawContext, MaterialCache);
	}

	void FDeferredShadingBasePassRenderer::RenderBasePassTranslucent(RenderCore::DynamicRHI* RHI, const std::vector<GltfSceneMeshInfo>& SceneMeshInfos,
																	 const FDeferredBasePassDrawContext& DrawContext, FMeshMaterialRenderCache& MaterialCache)
	{
		const math::Vector3 CamPos = DrawContext.ViewData ? DrawContext.ViewData->CameraPos : math::Vector3();
		std::vector<FTranslucentMeshSortKey> SortedKeys;
		FTranslucentMeshSorter::AppendSceneSortKeys(SceneMeshInfos, CamPos, SortedKeys);

		for (const auto& Key : SortedKeys)
		{
			const std::shared_ptr<MeshBase>& Mesh = Key.Mesh;
			if (Mesh->GetMaterial()->IsTransparent())
			{
				FDeferredBasePassMeshDispatch::Dispatch(RHI, Mesh, Key.WorldTransform, Key.PrevWorldTransform, MaterialCache.GetOrCreate(Mesh), true, DrawContext);
			}
		}
		for (const auto& Key : SortedKeys)
		{
			const std::shared_ptr<MeshBase>& Mesh = Key.Mesh;
			if (Mesh->GetMaterial()->IsTransparent())
			{
				FDeferredBasePassMeshDispatch::Dispatch(RHI, Mesh, Key.WorldTransform, Key.PrevWorldTransform, MaterialCache.GetOrCreate(Mesh), false, DrawContext);
			}
		}
	}

	void FDeferredShadingBasePassRenderer::RenderDeferredBasePassFullSequence(RenderCore::DynamicRHI* RHI, const std::vector<GltfSceneMeshInfo>& SceneMeshInfos,
																			  const FDeferredBasePassDrawContext& DrawContext, FMeshMaterialRenderCache& MaterialCache)
	{
		const math::Vector3 CamPos = DrawContext.ViewData ? DrawContext.ViewData->CameraPos : math::Vector3();
		FOpaqueMeshDrawBuilder::DrawSortedOpaqueMeshes(RHI, SceneMeshInfos, CamPos, true, DrawContext, MaterialCache);
		{
			std::vector<FTranslucentMeshSortKey> SortedKeys;
			FTranslucentMeshSorter::AppendSceneSortKeys(SceneMeshInfos, CamPos, SortedKeys);
			for (const auto& Key : SortedKeys)
			{
				const std::shared_ptr<MeshBase>& Mesh = Key.Mesh;
				if (Mesh->GetMaterial()->IsTransparent())
				{
					FDeferredBasePassMeshDispatch::Dispatch(RHI, Mesh, Key.WorldTransform, Key.PrevWorldTransform, MaterialCache.GetOrCreate(Mesh), true, DrawContext);
				}
			}
		}
		FOpaqueMeshDrawBuilder::DrawSortedOpaqueMeshes(RHI, SceneMeshInfos, CamPos, false, DrawContext, MaterialCache);
		{
			std::vector<FTranslucentMeshSortKey> SortedKeys;
			FTranslucentMeshSorter::AppendSceneSortKeys(SceneMeshInfos, CamPos, SortedKeys);
			for (const auto& Key : SortedKeys)
			{
				const std::shared_ptr<MeshBase>& Mesh = Key.Mesh;
				if (Mesh->GetMaterial()->IsTransparent())
				{
					FDeferredBasePassMeshDispatch::Dispatch(RHI, Mesh, Key.WorldTransform, Key.PrevWorldTransform, MaterialCache.GetOrCreate(Mesh), false, DrawContext);
				}
			}
		}
	}
}
