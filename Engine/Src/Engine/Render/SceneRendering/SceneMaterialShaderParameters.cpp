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
																				 const std::shared_ptr<FSceneTextures>& SceneTextures)
	{
		MaterialRenderParam Out;
		if (!ViewData || !WorldSceneRender || !Mesh)
			return Out;

		Out.lightInfos = ViewData->Lights;
		Out.PrimaryDirectionalLightIndex = -1;
		for (int32_t i = 0; i < static_cast<int32_t>(Out.lightInfos.size()); ++i)
		{
			if (Out.lightInfos[(size_t)i].Type == LightType_Directional)
			{
				Out.PrimaryDirectionalLightIndex = i;
				break;
			}
		}
		if (Out.PrimaryDirectionalLightIndex >= 0)
		{
			if (const std::shared_ptr<ShadowRenderPass> ShadowPass = WorldSceneRender->GetShadowRenderPass())
			{
				Light L{};
				if (ShadowPass->TryGetCachedMainLightForShading(L))
				{
					Light& Slot = Out.lightInfos[(size_t)Out.PrimaryDirectionalLightIndex];
					Slot.LightView = L.LightView;
					Slot.LightViewProj = L.LightViewProj;
					Slot.ShadowMapIndex = L.ShadowMapIndex;
					Slot.Position = L.Position;
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
		Out.skylightEnvironment = WorldSceneRender->GetUSkyLightComponent();
		Out.RotateIBL = math::Matrix4x4::ms_Materix3X3WIdentity;
		Out.SceneTextures = SceneTextures;
		Out.DrawMeshBuffer = Mesh->GetMeshBuffer();
		Out.bUnlit = ViewData->bUnlit;
		Out.SkyLightIBLScale = ViewData->SkyLightIBLScale;
		Out.IBLDiffuseDirShadowCoupling = ViewData->IBLDiffuseDirShadowCoupling;
		Out.IBLSpecularDirShadowCoupling = ViewData->IBLSpecularDirShadowCoupling;
		Out.IBLDiffuseAoExponentForIBL = ViewData->IBLDiffuseAoExponentForIBL;
		return Out;
	}
}
