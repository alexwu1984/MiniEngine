#include "Render/SceneRendering/FSceneMaterialShaderParameters.h"
#include "Render/FWorldSceneRender.h"
#include "Render/SceneRendering/FSceneViewData.h"
#include "Render/Shadow/ShadowRenderPass.h"
#include "Render/GBuffer.h"
#include "GltfModel/GltfMesh.h"

namespace Engine
{
	MaterialRenderParam FSceneMaterialShaderParameters::BuildForDeferredBasePass(const FWorldSceneRender* WorldSceneRender, const FSceneViewData* ViewData, const MeshBase* Mesh,
																				 const math::Matrix4x4& WorldTransform, const math::Matrix4x4& PrevWorldTransform,
																				 const std::shared_ptr<GBuffer>& TargetBuffer)
	{
		MaterialRenderParam Out;
		if (!ViewData || !WorldSceneRender || !Mesh)
			return Out;

		Out.lightInfos = ViewData->Lights;
		if (!Out.lightInfos.empty())
		{
			if (const std::shared_ptr<ShadowRenderPass> ShadowPass = WorldSceneRender->GetShadowRenderPass())
			{
				Light L{};
				if (ShadowPass->TryGetCachedMainLightForShading(L))
				{
					Out.lightInfos[0].LightView = L.LightView;
					Out.lightInfos[0].LightViewProj = L.LightViewProj;
					Out.lightInfos[0].ShadowMapIndex = L.ShadowMapIndex;
					Out.lightInfos[0].Position = L.Position;
				}
			}
		}
		Out.CameraPos = ViewData->CameraPos;
		Out.CurrModelMatrix = Mesh->GetMeshMat() * WorldTransform;
		Out.PrevModelMatrix = Mesh->GetMeshMat() * PrevWorldTransform;
		Out.CurrViewProjMatrix = ViewData->CurrViewProjMatrix;
		Out.CurrViewProjInverseMatrix = ViewData->CurrViewProjInverseMatrix;
		Out.PrevViewProjMatrix = ViewData->PrevViewProjMatrix;
		Out.PrevViewProjInverseMatrix = ViewData->PrevViewProjInverseMatrix;
		Out.TemporalAAJitter = ViewData->TemporalAAJitter;
		Out.HasSkin = Mesh->HasSkin();
		Out.preProcessor = WorldSceneRender->GetPreProcessor();
		math::Matrix4x4 Rotate = math::Matrix4x4::RotateX(math::Radians(ViewData->EnvironmentRotatePitchDegrees));
		Rotate *= math::Matrix4x4::RotateY(math::Radians(ViewData->EnvironmentRotateYawDegrees));
		Out.RotateIBL = Rotate;
		Out.TargetBuffer = TargetBuffer;
		Out.bUnlit = ViewData->bUnlit;
		Out.SkyLightIBLScale = ViewData->SkyLightIBLScale;
		return Out;
	}
}
