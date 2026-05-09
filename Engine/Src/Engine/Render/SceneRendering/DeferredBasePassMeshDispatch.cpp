#include "Render/SceneRendering/DeferredBasePassMeshDispatch.h"
#include "Render/SceneRendering/SceneMaterialShaderParameters.h"
#include "Render/SceneRendering/SceneViewData.h"
#include "Render/MaterialPreFrame.h"
#include "RHI/DynamicRHI.h"
#include "GltfModel/GltfMesh.h"

namespace Engine
{
	void FDeferredBasePassMeshDispatch::Dispatch(RenderCore::DynamicRHI* RHI, const std::shared_ptr<MeshBase>& Mesh, const math::Matrix4x4& WorldTransform,
												 const math::Matrix4x4& PrevWorldTransform, const std::shared_ptr<MaterialRender>& Material, bool bIsPrePass,
												 const FDeferredBasePassDrawContext& DrawContext)
	{
		const FSceneViewData* ViewData = DrawContext.ViewData ? DrawContext.ViewData.get() : nullptr;
		MaterialRenderParam Params = FSceneMaterialShaderParameters::BuildForDeferredBasePass(DrawContext.WorldSceneRender, ViewData, Mesh.get(), WorldTransform, PrevWorldTransform,
																							   DrawContext.SceneTextures);

		// Skinning VS reads cbPerSkeleton whenever the mesh has weights. PrePass used to skip uploads and left stale / zero matrices (garbage verts).
		if (Mesh->HasSkin())
		{
			const bool bResolvedPalette = Mesh->GetSkinId() > -1 && !Mesh->GetBoneNodeArray().empty()
				&& Mesh->GetSkinId() < static_cast<int>(Mesh->GetBoneNodeArray().size());
			if (bResolvedPalette)
			{
				auto& Bone = Mesh->GetBoneNodeArray()[static_cast<size_t>(Mesh->GetSkinId())];
				const uint32_t MaxSkin = static_cast<uint32_t>(CBPerSkeleton::kPaletteMatrixCount);
				const uint32_t NumBones = static_cast<uint32_t>(Bone.size());
				for (uint32_t BoneIndex = 0; BoneIndex < NumBones && BoneIndex < MaxSkin; ++BoneIndex)
					Material->SetBoneMatrix(Bone[BoneIndex].FinalMat, static_cast<int32_t>(BoneIndex));
				Material->OnSkinnedPaletteUploaded(static_cast<int32_t>(NumBones));
			}
			else
				Material->ResetSkeletonPaletteIdentity();
		}

		RenderCore::RHICommandContext* CmdList = DrawContext.RHICmdList;
		if (!CmdList)
		{
			auto DefaultCtx = RHI->GetDefaultCommandContext();
			CmdList = DefaultCtx ? DefaultCtx.get() : nullptr;
		}
		if (!CmdList)
			return;

		if (bIsPrePass)
			Material->PreDraw(*CmdList, Params);
		else
			Material->Draw(*CmdList, Params);
	}
}
