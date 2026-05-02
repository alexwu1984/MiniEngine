#include "Render/SceneRendering/DeferredShadingBasePassRenderer.h"
#include "Render/SceneRendering/OpaqueMeshDrawBuilder.h"
#include "Render/SceneRendering/DeferredBasePassMeshDispatch.h"
#include "Render/SceneRendering/MeshMaterialRenderCache.h"
#include "Render/SceneRendering/TranslucentMeshSorter.h"
#include "Render/SceneRendering/TranslucentMeshSortKey.h"
#include "Render/SceneRendering/SceneViewData.h"
#include "Render/WorldSceneRender.h"
#include "RHI/DynamicRHI.h"
#include "Scene/SceneMeshComponent.h"
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
		const uint64_t SceneGen = DrawContext.WorldSceneRender ? DrawContext.WorldSceneRender->GetMeshMaterialCacheSceneGeneration() : 0u;
		std::vector<FTranslucentMeshSortKey> SortedKeys;
		FTranslucentMeshSorter::AppendSceneSortKeys(SceneMeshInfos, CamPos, SortedKeys);

		for (const auto& Key : SortedKeys)
		{
			const std::shared_ptr<MeshBase>& Mesh = Key.Mesh;
			if (Mesh->GetMaterial()->IsTransparent())
			{
				FDeferredBasePassMeshDispatch::Dispatch(RHI, Mesh, Key.WorldTransform, Key.PrevWorldTransform, MaterialCache.GetOrCreate(Mesh, Key.MaterialRenderCacheKey, SceneGen), true, DrawContext);
			}
		}
		for (const auto& Key : SortedKeys)
		{
			const std::shared_ptr<MeshBase>& Mesh = Key.Mesh;
			if (Mesh->GetMaterial()->IsTransparent())
			{
				FDeferredBasePassMeshDispatch::Dispatch(RHI, Mesh, Key.WorldTransform, Key.PrevWorldTransform, MaterialCache.GetOrCreate(Mesh, Key.MaterialRenderCacheKey, SceneGen), false, DrawContext);
			}
		}
	}

	void FDeferredShadingBasePassRenderer::RenderDeferredBasePassFullSequence(RenderCore::DynamicRHI* RHI, const std::vector<GltfSceneMeshInfo>& SceneMeshInfos,
																			  const FDeferredBasePassDrawContext& DrawContext, FMeshMaterialRenderCache& MaterialCache)
	{
		const math::Vector3 CamPos = DrawContext.ViewData ? DrawContext.ViewData->CameraPos : math::Vector3();
		const uint64_t SceneGen = DrawContext.WorldSceneRender ? DrawContext.WorldSceneRender->GetMeshMaterialCacheSceneGeneration() : 0u;
		FOpaqueMeshDrawBuilder::DrawSortedOpaqueMeshes(RHI, SceneMeshInfos, CamPos, true, DrawContext, MaterialCache);
		{
			std::vector<FTranslucentMeshSortKey> SortedKeys;
			FTranslucentMeshSorter::AppendSceneSortKeys(SceneMeshInfos, CamPos, SortedKeys);
			for (const auto& Key : SortedKeys)
			{
				const std::shared_ptr<MeshBase>& Mesh = Key.Mesh;
				if (Mesh->GetMaterial()->IsTransparent())
				{
					FDeferredBasePassMeshDispatch::Dispatch(RHI, Mesh, Key.WorldTransform, Key.PrevWorldTransform, MaterialCache.GetOrCreate(Mesh, Key.MaterialRenderCacheKey, SceneGen), true, DrawContext);
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
					FDeferredBasePassMeshDispatch::Dispatch(RHI, Mesh, Key.WorldTransform, Key.PrevWorldTransform, MaterialCache.GetOrCreate(Mesh, Key.MaterialRenderCacheKey, SceneGen), false, DrawContext);
				}
			}
		}
	}
}
