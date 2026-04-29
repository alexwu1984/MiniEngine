#include "Render/SceneRendering/FDeferredBasePassMeshDispatch.h"
#include "Render/SceneRendering/FSceneMaterialShaderParameters.h"
#include "RHI/DynamicRHI.h"
#include "Scene/CameraComponent.h"
#include "GltfModel/GltfMesh.h"

namespace Engine
{
	void FDeferredBasePassMeshDispatch::Dispatch(RenderCore::DynamicRHI* RHI, const std::shared_ptr<MeshBase>& Mesh, const math::Matrix4x4& WorldTransform,
												 const math::Matrix4x4& PrevWorldTransform, const std::shared_ptr<MaterialRender>& Material, const std::shared_ptr<CameraComponent>& Camera,
												 bool bIsPrePass, const FDeferredBasePassDrawContext& DrawContext)
	{
		MaterialRenderParam Params = FSceneMaterialShaderParameters::BuildForDeferredBasePass(
			DrawContext.SceneRenderRaw, DrawContext.View.get(), Camera.get(), Mesh.get(), WorldTransform, PrevWorldTransform, DrawContext.TargetBuffer,
			DrawContext.EnvironmentRotatePitchDegrees, DrawContext.EnvironmentRotateYawDegrees);

		if (!bIsPrePass && Mesh->GetSkinId() > -1 && Mesh->GetBoneNodeArray().size() > 0)
		{
			auto& Bone = Mesh->GetBoneNodeArray()[Mesh->GetSkinId()];
			for (uint32_t BoneIndex = 0; BoneIndex < Bone.size(); BoneIndex++)
			{
				Material->SetBoneMatrix(Bone[BoneIndex].FinalMat, BoneIndex);
			}
		}

		if (bIsPrePass)
			Material->PreDraw(*RHI->GetDefaultCommandContext(), Params);
		else
			Material->Draw(*RHI->GetDefaultCommandContext(), Params);
	}
}
