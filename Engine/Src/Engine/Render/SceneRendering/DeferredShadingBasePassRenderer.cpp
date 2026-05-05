#include "Render/SceneRendering/DeferredShadingBasePassRenderer.h"
#include "Render/SceneRendering/OpaqueMeshDrawBuilder.h"
#include "Render/SceneRendering/DeferredBasePassMeshDispatch.h"
#include "Render/SceneRendering/MeshMaterialRenderCache.h"
#include "Render/SceneRendering/TranslucentMeshSorter.h"
#include "Render/SceneRendering/TranslucentMeshSortKey.h"
#include "Render/SceneRendering/SceneViewData.h"
#include "Render/SceneRendering/SceneMaterialShaderParameters.h"
#include "Render/SceneRendering/DeferredLightingPass.h"
#include "Render/RDGUtils.h"
#include "Render/SceneTextures.h"
#include "RHI/DynamicRHI.h"
#include "Scene/SceneMeshComponent.h"
#include "GltfModel/GltfMesh.h"
#include "Material/GltfMaterial.h"
#include "Material/MaterialBase.h"
#include "Engine/Render/FurMaterialRender.h"
#include "Engine/Render/MaterialRender.h"

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
				FDeferredBasePassMeshDispatch::Dispatch(RHI, Mesh, Key.WorldTransform, Key.PrevWorldTransform,
														MaterialCache.GetOrCreate(Mesh, Key.MaterialRenderCacheKey), true, DrawContext);
			}
		}
		for (const auto& Key : SortedKeys)
		{
			const std::shared_ptr<MeshBase>& Mesh = Key.Mesh;
			if (Mesh->GetMaterial()->IsTransparent())
			{
				FDeferredBasePassMeshDispatch::Dispatch(RHI, Mesh, Key.WorldTransform, Key.PrevWorldTransform,
														MaterialCache.GetOrCreate(Mesh, Key.MaterialRenderCacheKey), false, DrawContext);
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
					FDeferredBasePassMeshDispatch::Dispatch(RHI, Mesh, Key.WorldTransform, Key.PrevWorldTransform,
														MaterialCache.GetOrCreate(Mesh, Key.MaterialRenderCacheKey), true, DrawContext);
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
					FDeferredBasePassMeshDispatch::Dispatch(RHI, Mesh, Key.WorldTransform, Key.PrevWorldTransform,
														MaterialCache.GetOrCreate(Mesh, Key.MaterialRenderCacheKey), false, DrawContext);
				}
			}
		}
	}

	void FDeferredShadingBasePassRenderer::RenderFurForwardAfterDeferredLighting(RenderCore::DynamicRHI* RHI, const std::vector<GltfSceneMeshInfo>& SceneMeshInfos,
																				 const FDeferredBasePassDrawContext& DrawContext, FMeshMaterialRenderCache& MaterialCache,
																				 DeferredLightingPass* DeferredLighting)
	{
		(void)RHI;
		if (!DrawContext.RHICmdList || !DrawContext.ViewData || !DrawContext.TargetBuffer || !DrawContext.WorldSceneRender)
			return;
		if (DrawContext.ViewData->bUnlit || !DeferredLighting)
			return;

		const math::Vector3 CamPos = DrawContext.ViewData->CameraPos;
		std::vector<FTranslucentMeshSortKey> Flat;
		for (const auto& SceneMeshInfo : SceneMeshInfos)
			FTranslucentMeshSorter::AppendPerActorMeshSortKeys(SceneMeshInfo, CamPos, Flat);

		bool anyFur = false;
		for (const auto& Key : Flat)
		{
			const std::shared_ptr<MeshBase>& Mesh = Key.Mesh;
			const std::shared_ptr<MaterialBase> meshMat = Mesh ? Mesh->GetMaterial() : nullptr;
			if (!Mesh || !meshMat || meshMat->IsTransparent())
				continue;
			std::shared_ptr<MaterialRender> Mat = MaterialCache.GetOrCreate(Mesh, Key.MaterialRenderCacheKey);
			if (dynamic_cast<FurMaterialRender*>(Mat.get()))
			{
				anyFur = true;
				break;
			}
		}
		if (!anyFur)
			return;

		RenderCore::RHICommandContext& Cmd = *DrawContext.RHICmdList;
		FRDGUtils::RHICmdListSetViewportFromTexture(Cmd, DrawContext.TargetBuffer->GetSceneColor());

		for (const auto& Key : Flat)
		{
			const std::shared_ptr<MeshBase>& Mesh = Key.Mesh;
			const std::shared_ptr<MaterialBase> meshMat = Mesh ? Mesh->GetMaterial() : nullptr;
			if (!Mesh || !meshMat || meshMat->IsTransparent())
				continue;
			std::shared_ptr<MaterialRender> Mat = MaterialCache.GetOrCreate(Mesh, Key.MaterialRenderCacheKey);
			if (auto* fur = dynamic_cast<FurMaterialRender*>(Mat.get()))
			{
				MaterialRenderParam P = FSceneMaterialShaderParameters::BuildForDeferredBasePass(
					DrawContext.WorldSceneRender, DrawContext.ViewData.get(), Mesh.get(), Key.WorldTransform, Key.PrevWorldTransform, DrawContext.TargetBuffer);
				fur->DrawForwardFur(Cmd, P, DeferredLighting, DrawContext.WorldSceneRender, DrawContext.ViewData);
			}
		}
	}
}
