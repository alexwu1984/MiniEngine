#include "Render/SceneRendering/FSceneMaterialShaderParameters.h"
#include "Render/SceneRender.h"
#include "Render/SceneRendering/FSceneViewData.h"
#include "Render/GBuffer.h"
#include "GltfModel/GltfMesh.h"

namespace Engine
{
	MaterialRenderParam FSceneMaterialShaderParameters::BuildForDeferredBasePass(const SceneRender* SceneRender, const FSceneViewData* ViewData, const MeshBase* Mesh,
																				 const math::Matrix4x4& WorldTransform, const math::Matrix4x4& PrevWorldTransform,
																				 const std::shared_ptr<GBuffer>& TargetBuffer)
	{
		MaterialRenderParam Out;
		if (!ViewData || !SceneRender || !Mesh)
			return Out;

		Out.lightInfos = ViewData->Lights;
		Out.CameraPos = ViewData->CameraPos;
		Out.CurrModelMatrix = Mesh->GetMeshMat() * WorldTransform;
		Out.PrevModelMatrix = Mesh->GetMeshMat() * PrevWorldTransform;
		Out.CurrViewProjMatrix = ViewData->CurrViewProjMatrix;
		Out.CurrViewProjInverseMatrix = ViewData->CurrViewProjInverseMatrix;
		Out.PrevViewProjMatrix = ViewData->PrevViewProjMatrix;
		Out.PrevViewProjInverseMatrix = ViewData->PrevViewProjInverseMatrix;
		Out.TemporalAAJitter = ViewData->TemporalAAJitter;
		Out.HasSkin = Mesh->HasSkin();
		Out.preProcessor = SceneRender->GetPreProcessor();
		math::Matrix4x4 Rotate = math::Matrix4x4::RotateX(math::Radians(ViewData->EnvironmentRotatePitchDegrees));
		Rotate *= math::Matrix4x4::RotateY(math::Radians(ViewData->EnvironmentRotateYawDegrees));
		Out.RotateIBL = Rotate;
		Out.TargetBuffer = TargetBuffer;
		return Out;
	}
}
