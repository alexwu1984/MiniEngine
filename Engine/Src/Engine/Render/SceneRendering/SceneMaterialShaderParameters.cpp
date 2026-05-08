#include "Render/SceneRendering/SceneMaterialShaderParameters.h"
#include "Render/MaterialPreFrame.h"
#include "Render/WorldSceneRender.h"
#include "Render/SceneRendering/SceneViewData.h"
#include "Render/Shadow/ShadowRenderPass.h"
#include "Render/SceneTextures.h"
#include "GltfModel/GltfMesh.h"

namespace Engine
{
	MaterialRenderParam FSceneMaterialShaderParameters::BuildForDeferredBasePass(const FWorldSceneRender* WorldSceneRender, const FSceneViewData* ViewData, const MeshBase* Mesh,
																				 const math::Matrix4x4& WorldTransform, const math::Matrix4x4& PrevWorldTransform,
																				 const std::shared_ptr<SceneTextures>& TargetBuffer)
	{
		MaterialRenderParam Out;
		if (!ViewData || !WorldSceneRender || !Mesh)
			return Out;

		Out.lightInfos = ViewData->Lights;
		if (!Out.lightInfos.empty() && Out.lightInfos[0].Type == LightType_Directional)
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
		Out.RotateIBL = math::Matrix4x4::ms_Materix3X3WIdentity;
		Out.TargetBuffer = TargetBuffer;
		Out.DrawMeshBuffer = Mesh->GetMeshBuffer();
		Out.bUnlit = ViewData->bUnlit;
		Out.SkyLightIBLScale = ViewData->SkyLightIBLScale;
		return Out;
	}
}
