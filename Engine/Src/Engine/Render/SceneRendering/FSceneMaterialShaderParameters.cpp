#include "Render/SceneRendering/FSceneMaterialShaderParameters.h"
#include "Render/SceneRender.h"
#include "Scene/SceneView.h"
#include "Scene/CameraComponent.h"
#include "Render/GBuffer.h"
#include "GltfModel/GltfMesh.h"

namespace Engine
{
	MaterialRenderParam FSceneMaterialShaderParameters::BuildForDeferredBasePass(const SceneRender* SceneRender, const SceneView* View, CameraComponent* Camera,
																				 const MeshBase* Mesh, const math::Matrix4x4& WorldTransform, const math::Matrix4x4& PrevWorldTransform,
																				 const std::shared_ptr<GBuffer>& TargetBuffer, float EnvironmentRotatePitchDegrees,
																				 float EnvironmentRotateYawDegrees)
	{
		MaterialRenderParam Out;
		Out.lightInfos = View->GetLights();
		Out.CameraPos = Camera->GetCameraPos();
		Out.CurrModelMatrix = Mesh->GetMeshMat() * WorldTransform;
		Out.PrevModelMatrix = Mesh->GetMeshMat() * PrevWorldTransform;
		if (SceneRender->UsesTemporalAAProjectionJitter())
			Out.CurrViewProjMatrix = Camera->GetViewMatrix() * Camera->HackAddTemporalAAProjectionJitter(false);
		else
			Out.CurrViewProjMatrix = Camera->GetViewMatrix() * Camera->GetProjMatrix();
		Out.CurrViewProjInverseMatrix = Out.CurrViewProjMatrix.Inverse();
		if (SceneRender->UsesTemporalAAProjectionJitter())
			Out.PrevViewProjMatrix = Camera->GetPrevViewMatrix() * Camera->HackAddTemporalAAProjectionJitter(true);
		else
			Out.PrevViewProjMatrix = Camera->GetPrevViewMatrix() * Camera->GetPrevProjMatrix();
		Out.PrevViewProjInverseMatrix = Out.PrevViewProjMatrix.Inverse();
		Out.TemporalAAJitter = Camera->GetTemporalAAJitter();
		Out.HasSkin = Mesh->HasSkin();
		Out.preProcessor = SceneRender->GetPreProcessor();
		math::Matrix4x4 Rotate = math::Matrix4x4::RotateX(math::Radians(EnvironmentRotatePitchDegrees));
		Rotate *= math::Matrix4x4::RotateY(math::Radians(EnvironmentRotateYawDegrees));
		Out.RotateIBL = Rotate;
		Out.TargetBuffer = TargetBuffer;
		return Out;
	}
}
