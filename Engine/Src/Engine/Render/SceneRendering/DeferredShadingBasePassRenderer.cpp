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
#include "Scene/SceneMeshComponent.h"
#include "GltfModel/GltfMesh.h"
#include "Material/GltfMaterial.h"
#include "Material/MaterialBase.h"
#include "Engine/Render/FurMaterialRender.h"
#include "Engine/Render/PBRMaterialRender.h"
#include "Engine/Render/MaterialRender.h"
#include "RHI/DynamicRHI.h"

namespace Engine
{
	void FDeferredShadingBasePassRenderer::RenderBasePassOpaque(const FDeferredBasePassDrawContext& DrawContext)
	{
		FOpaqueMeshDrawBuilder::DrawSortedOpaqueMeshes(DrawContext, true);
		FOpaqueMeshDrawBuilder::DrawSortedOpaqueMeshes(DrawContext, false);
	}

	void FDeferredShadingBasePassRenderer::RenderBasePassTranslucent(const FDeferredBasePassDrawContext& DrawContext)
	{
		if (!DrawContext.MeshesForDraw || !DrawContext.MaterialCache)
			return;
		const std::vector<GltfSceneMeshInfo>& SceneMeshInfos = *DrawContext.MeshesForDraw;
		FMeshMaterialRenderCache& MaterialCache = *DrawContext.MaterialCache;
		const math::Vector3 CamPos = DrawContext.ViewData ? DrawContext.ViewData->CameraPos : math::Vector3();

		std::vector<FTranslucentMeshSortKey> SortedKeys;
		FTranslucentMeshSorter::AppendSceneSortKeys(SceneMeshInfos, CamPos, SortedKeys);

		for (const auto& Key : SortedKeys)
		{
			const std::shared_ptr<MeshBase>& Mesh = Key.Mesh;
			if (Mesh->GetMaterial()->IsTransparent())
			{
				FDeferredBasePassMeshDispatch::Dispatch(Mesh, Key.WorldTransform, Key.PrevWorldTransform,
														MaterialCache.GetOrCreate(Mesh, Key.MaterialRenderCacheKey), true, DrawContext);
			}
		}
		for (const auto& Key : SortedKeys)
		{
			const std::shared_ptr<MeshBase>& Mesh = Key.Mesh;
			if (Mesh->GetMaterial()->IsTransparent())
			{
				FDeferredBasePassMeshDispatch::Dispatch(Mesh, Key.WorldTransform, Key.PrevWorldTransform,
														MaterialCache.GetOrCreate(Mesh, Key.MaterialRenderCacheKey), false, DrawContext);
			}
		}
	}

	void FDeferredShadingBasePassRenderer::RenderDeferredBasePassFullSequence(const FDeferredBasePassDrawContext& DrawContext)
	{
		if (!DrawContext.MeshesForDraw || !DrawContext.MaterialCache)
			return;
		const std::vector<GltfSceneMeshInfo>& SceneMeshInfos = *DrawContext.MeshesForDraw;
		FMeshMaterialRenderCache& MaterialCache = *DrawContext.MaterialCache;
		const math::Vector3 CamPos = DrawContext.ViewData ? DrawContext.ViewData->CameraPos : math::Vector3();

		FOpaqueMeshDrawBuilder::DrawSortedOpaqueMeshes(DrawContext, true);
		{
			std::vector<FTranslucentMeshSortKey> SortedKeys;
			FTranslucentMeshSorter::AppendSceneSortKeys(SceneMeshInfos, CamPos, SortedKeys);
			for (const auto& Key : SortedKeys)
			{
				const std::shared_ptr<MeshBase>& Mesh = Key.Mesh;
				if (Mesh->GetMaterial()->IsTransparent())
				{
					FDeferredBasePassMeshDispatch::Dispatch(Mesh, Key.WorldTransform, Key.PrevWorldTransform,
														MaterialCache.GetOrCreate(Mesh, Key.MaterialRenderCacheKey), true, DrawContext);
				}
			}
		}
		FOpaqueMeshDrawBuilder::DrawSortedOpaqueMeshes(DrawContext, false);
		{
			std::vector<FTranslucentMeshSortKey> SortedKeys;
			FTranslucentMeshSorter::AppendSceneSortKeys(SceneMeshInfos, CamPos, SortedKeys);
			for (const auto& Key : SortedKeys)
			{
				const std::shared_ptr<MeshBase>& Mesh = Key.Mesh;
				if (Mesh->GetMaterial()->IsTransparent())
				{
					FDeferredBasePassMeshDispatch::Dispatch(Mesh, Key.WorldTransform, Key.PrevWorldTransform,
														MaterialCache.GetOrCreate(Mesh, Key.MaterialRenderCacheKey), false, DrawContext);
				}
			}
		}
	}

	void FDeferredShadingBasePassRenderer::RenderTranslucentForward(const FDeferredBasePassDrawContext& DrawContext)
	{
		if (!DrawContext.RHICmdList || !DrawContext.ViewData || !DrawContext.SceneTextures || !DrawContext.WorldSceneRender || !DrawContext.MeshesForDraw || !DrawContext.MaterialCache)
			return;
		if (DrawContext.ViewData->bUnlit || !DrawContext.DeferredLighting)
			return;
		const std::vector<GltfSceneMeshInfo>& SceneMeshInfos = *DrawContext.MeshesForDraw;
		const math::Vector3 CamPos = DrawContext.ViewData->CameraPos;
		FMeshMaterialRenderCache& MaterialCache = *DrawContext.MaterialCache;
		std::vector<FTranslucentMeshSortKey> SortedKeys;
		FTranslucentMeshSorter::AppendSceneSortKeys(SceneMeshInfos, CamPos, SortedKeys);

		RenderCore::RHICommandContext& Cmd = *DrawContext.RHICmdList;
		FRDGUtils::RHICmdListSetViewportFromTexture(Cmd, DrawContext.SceneTextures->GetSceneColor());

		for (const auto& Key : SortedKeys)
		{
			const std::shared_ptr<MeshBase>& Mesh = Key.Mesh;
			const std::shared_ptr<MaterialBase> meshMat = Mesh ? Mesh->GetMaterial() : nullptr;
			if (!Mesh || !meshMat || !meshMat->IsTransparent())
				continue;
			std::shared_ptr<MaterialRender> Mat = MaterialCache.GetOrCreate(Mesh, Key.MaterialRenderCacheKey);
			if (dynamic_cast<FurMaterialRender*>(Mat.get()))
				continue;
			auto* pbr = dynamic_cast<PBRMaterialRender*>(Mat.get());
			if (!pbr)
				continue;
			MaterialRenderParam P = FSceneMaterialShaderParameters::BuildForDeferredBasePass(
				DrawContext.WorldSceneRender, DrawContext.ViewData.get(), Mesh.get(), Key.WorldTransform, Key.PrevWorldTransform, DrawContext.SceneTextures);
			pbr->DrawTranslucentForwardLit(Cmd, P, DrawContext.DeferredLighting, DrawContext.WorldSceneRender, DrawContext.ViewData);
		}
	}

	void FDeferredShadingBasePassRenderer::RenderFurForward(const FDeferredBasePassDrawContext& DrawContext)
	{
		if (!DrawContext.RHICmdList || !DrawContext.ViewData || !DrawContext.SceneTextures || !DrawContext.WorldSceneRender || !DrawContext.MeshesForDraw || !DrawContext.MaterialCache)
			return;
		if (DrawContext.ViewData->bUnlit || !DrawContext.DeferredLighting)
			return;

		const std::vector<GltfSceneMeshInfo>& SceneMeshInfos = *DrawContext.MeshesForDraw;
		const math::Vector3 CamPos = DrawContext.ViewData->CameraPos;
		FMeshMaterialRenderCache& MaterialCache = *DrawContext.MaterialCache;
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
		FRDGUtils::RHICmdListSetViewportFromTexture(Cmd, DrawContext.SceneTextures->GetSceneColor());

		const std::vector<std::shared_ptr<RenderCore::RHITexture2D>> FurPassRt = { DrawContext.SceneTextures->GetSceneColor() };
		Cmd.SetRenderTarget(FurPassRt, DrawContext.SceneTextures->GetDepth());

		uintptr_t furSharedBoundPsKey = 0u;
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
					DrawContext.WorldSceneRender, DrawContext.ViewData.get(), Mesh.get(), Key.WorldTransform, Key.PrevWorldTransform, DrawContext.SceneTextures);
				fur->DrawForwardFur(Cmd, P, &furSharedBoundPsKey, DrawContext.DeferredLighting, DrawContext.WorldSceneRender, DrawContext.ViewData);
			}
		}
	}
}
