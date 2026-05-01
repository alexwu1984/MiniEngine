#include "Render/SceneRendering/DeferredBasePassMeshDispatch.h"
#include "Render/SceneRendering/SceneMaterialShaderParameters.h"
#include "Render/SceneRendering/SceneViewData.h"
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
																							   DrawContext.TargetBuffer);

		if (!bIsPrePass && Mesh->GetSkinId() > -1 && Mesh->GetBoneNodeArray().size() > 0)
		{
			auto& Bone = Mesh->GetBoneNodeArray()[Mesh->GetSkinId()];
			for (uint32_t BoneIndex = 0; BoneIndex < Bone.size(); BoneIndex++)
			{
				Material->SetBoneMatrix(Bone[BoneIndex].FinalMat, BoneIndex);
			}
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
