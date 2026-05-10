#include "Render/SceneRendering/SceneViewData.h"
#include "Scene/CameraComponent.h"

namespace Engine
{
	void FSceneViewData::BuildFromCamera(CameraComponent& Camera, std::vector<Light> InLights, bool bUseHaltonProjectionJitterInViewMatrices,
										 int32_t ViewRectX, int32_t ViewRectY, int32_t ViewRectW, int32_t ViewRectH)
	{
		ViewFrustum = Camera.GetFrustum();
		ViewMatrix = Camera.GetViewMatrix();
		PrevViewMatrix = Camera.GetPrevViewMatrix();
		ProjMatrix = Camera.GetProjMatrix();
		PrevProjMatrix = Camera.GetPrevProjMatrix();
		CameraPos = Camera.GetCameraPos();
		CameraNearZ = Camera.GetNearPlane();
		CameraFarZ = Camera.GetFarPlane();
		TemporalAAJitter = Camera.GetTemporalAAJitter();
		FrameIndexMod2 = Camera.GetFrameIndexMod2();
		FrameIndex = Camera.GetFrameIndex();
		TemporalHistoryGeneration = Camera.GetTemporalHistoryGeneration();
		Lights = std::move(InLights);
		bHaltonProjectionJitterForMainPass = bUseHaltonProjectionJitterInViewMatrices;
		ViewRectMinX = ViewRectX;
		ViewRectMinY = ViewRectY;
		ViewRectSizeX = ViewRectW;
		ViewRectSizeY = ViewRectH;

		SsrViewProjMatrix = ViewMatrix * ProjMatrix;
		SsrInvViewProjMatrix = SsrViewProjMatrix.Inverse();

		if (bUseHaltonProjectionJitterInViewMatrices)
		{
			CurrViewProjMatrix = ViewMatrix * Camera.HackAddTemporalAAProjectionJitter(false);
			PrevViewProjMatrix = PrevViewMatrix * Camera.HackAddTemporalAAProjectionJitter(true);
		}
		else
		{
			CurrViewProjMatrix = SsrViewProjMatrix;
			PrevViewProjMatrix = PrevViewMatrix * PrevProjMatrix;
		}
		CurrViewProjInverseMatrix = CurrViewProjMatrix.Inverse();
		PrevViewProjInverseMatrix = PrevViewProjMatrix.Inverse();

		math::Matrix4x4 ViewSky = ViewMatrix;
		ViewSky._30 = ViewSky._31 = ViewSky._32 = 0.f;
		math::Matrix4x4 ProjForSky = ProjMatrix;
		if (bUseHaltonProjectionJitterInViewMatrices)
			ProjForSky = Camera.HackAddTemporalAAProjectionJitter(false);
		SkyInverseViewProj = (ViewSky * ProjForSky).Inverse();
	}
}
