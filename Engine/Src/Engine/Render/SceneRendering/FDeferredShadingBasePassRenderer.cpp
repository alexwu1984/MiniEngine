#include "Render/SceneRendering/FDeferredShadingBasePassRenderer.h"
#include "Render/SceneRendering/FDeferredBasePassMeshDispatch.h"
#include "Render/SceneRendering/FOpaqueMeshDrawBuilder.h"
#include "Render/SceneRendering/FMeshMaterialRenderCache.h"
#include "Render/SceneRendering/FTranslucentMeshSorter.h"
#include "Render/SceneRendering/FTranslucentMeshSortKey.h"
#include "RHI/DynamicRHI.h"
#include "Scene/SceneView.h"
#include "Scene/CameraComponent.h"
#include "Scene/GltfMeshComponent.h"
#include "GltfModel/GltfMesh.h"
#include "Material/GltfMaterial.h"

namespace Engine
{
	void FDeferredShadingBasePassRenderer::RenderBasePassOpaque(RenderCore::DynamicRHI* RHI, const std::vector<GltfSceneMeshInfo>& SceneMeshInfos, const FDeferredBasePassDrawContext& DrawContext,
															  FMeshMaterialRenderCache& MaterialCache)
	{
		const std::shared_ptr<CameraComponent> Camera = DrawContext.View->GetMainCamera();
		FOpaqueMeshDrawBuilder::DrawSortedOpaqueMeshes(RHI, SceneMeshInfos, Camera, true, DrawContext, MaterialCache);
		FOpaqueMeshDrawBuilder::DrawSortedOpaqueMeshes(RHI, SceneMeshInfos, Camera, false, DrawContext, MaterialCache);
	}

	void FDeferredShadingBasePassRenderer::RenderBasePassTranslucent(RenderCore::DynamicRHI* RHI, const std::vector<GltfSceneMeshInfo>& SceneMeshInfos,
																	const FDeferredBasePassDrawContext& DrawContext, FMeshMaterialRenderCache& MaterialCache)
	{
		const std::shared_ptr<CameraComponent> Camera = DrawContext.View->GetMainCamera();
		std::vector<FTranslucentMeshSortKey> SortedKeys;
		FTranslucentMeshSorter::AppendSceneSortKeys(SceneMeshInfos, Camera->GetCameraPos(), SortedKeys);

		for (const auto& Key : SortedKeys)
		{
			const std::shared_ptr<MeshBase>& Mesh = Key.Mesh;
			if (Mesh->GetMaterial()->IsTransparent())
			{
				FDeferredBasePassMeshDispatch::Dispatch(RHI, Mesh, Key.WorldTransform, Key.PrevWorldTransform, MaterialCache.GetOrCreate(Mesh), Camera, true, DrawContext);
			}
		}
		for (const auto& Key : SortedKeys)
		{
			const std::shared_ptr<MeshBase>& Mesh = Key.Mesh;
			if (Mesh->GetMaterial()->IsTransparent())
			{
				FDeferredBasePassMeshDispatch::Dispatch(RHI, Mesh, Key.WorldTransform, Key.PrevWorldTransform, MaterialCache.GetOrCreate(Mesh), Camera, false, DrawContext);
			}
		}
	}

	void FDeferredShadingBasePassRenderer::RenderDeferredBasePassFullSequence(RenderCore::DynamicRHI* RHI, const std::vector<GltfSceneMeshInfo>& SceneMeshInfos,
																			  const FDeferredBasePassDrawContext& DrawContext, FMeshMaterialRenderCache& MaterialCache)
	{
		const std::shared_ptr<CameraComponent> Camera = DrawContext.View->GetMainCamera();
		FOpaqueMeshDrawBuilder::DrawSortedOpaqueMeshes(RHI, SceneMeshInfos, Camera, true, DrawContext, MaterialCache);
		{
			std::vector<FTranslucentMeshSortKey> SortedKeys;
			FTranslucentMeshSorter::AppendSceneSortKeys(SceneMeshInfos, Camera->GetCameraPos(), SortedKeys);
			for (const auto& Key : SortedKeys)
			{
				const std::shared_ptr<MeshBase>& Mesh = Key.Mesh;
				if (Mesh->GetMaterial()->IsTransparent())
				{
					FDeferredBasePassMeshDispatch::Dispatch(RHI, Mesh, Key.WorldTransform, Key.PrevWorldTransform, MaterialCache.GetOrCreate(Mesh), Camera, true, DrawContext);
				}
			}
		}
		FOpaqueMeshDrawBuilder::DrawSortedOpaqueMeshes(RHI, SceneMeshInfos, Camera, false, DrawContext, MaterialCache);
		{
			std::vector<FTranslucentMeshSortKey> SortedKeys;
			FTranslucentMeshSorter::AppendSceneSortKeys(SceneMeshInfos, Camera->GetCameraPos(), SortedKeys);
			for (const auto& Key : SortedKeys)
			{
				const std::shared_ptr<MeshBase>& Mesh = Key.Mesh;
				if (Mesh->GetMaterial()->IsTransparent())
				{
					FDeferredBasePassMeshDispatch::Dispatch(RHI, Mesh, Key.WorldTransform, Key.PrevWorldTransform, MaterialCache.GetOrCreate(Mesh), Camera, false, DrawContext);
				}
			}
		}
	}
}
