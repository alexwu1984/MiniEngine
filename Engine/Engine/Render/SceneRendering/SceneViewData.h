#pragma once
#include "math/frustum.h"
#include "math/matrix4x4.h"
#include "math/vector3.h"
#include "math/vector4.h"
#include "Render/MaterialPreFrame.h"
#include <cstdint>
#include <vector>

namespace Engine
{
	class CameraComponent;

	/** Immutable per-frame view state for rendering (UE FSceneView analogue). Built on the game thread after camera tick. */
	struct FSceneViewData
	{
		math::Frustum ViewFrustum{};
		math::Matrix4x4 ViewMatrix{};
		math::Matrix4x4 PrevViewMatrix{};
		math::Matrix4x4 ProjMatrix{};
		math::Matrix4x4 PrevProjMatrix{};
		math::Matrix4x4 CurrViewProjMatrix{};
		math::Matrix4x4 PrevViewProjMatrix{};
		math::Matrix4x4 CurrViewProjInverseMatrix{};
		math::Matrix4x4 PrevViewProjInverseMatrix{};
		math::Matrix4x4 SsrViewProjMatrix{};
		math::Matrix4x4 SsrInvViewProjMatrix{};
		math::Vector3 CameraPos{};
		float CameraNearZ = 0.1f;
		float CameraFarZ = 1000.f;
		math::Vector4 TemporalAAJitter{ 1.f, 1.f, 1.f, 1.f };
		int32_t FrameIndexMod2 = 0;
		int32_t FrameIndex = 0;
		uint32_t TemporalHistoryGeneration = 0;
		std::vector<Light> Lights;
		/** True when main pass used jittered projection matrices for temporal resolve (driven by post AA policy). */
		bool bHaltonProjectionJitterForMainPass = false;
		int32_t ViewRectMinX = 0;
		int32_t ViewRectMinY = 0;
		int32_t ViewRectSizeX = 0;
		int32_t ViewRectSizeY = 0;

		/** View-wide unlit (UE EngineShowFlags::Unlit style): base color + emissive only, no lit BRDF / IBL. */
		bool bUnlit = false;

		/** 0 = no skylight IBL (UE Skylight off); primary SkyLightComponent drives HDR path and intensity. */
		float SkyLightIBLScale = 0.f;

		void BuildFromCamera(CameraComponent& Camera, std::vector<Light> InLights, bool bUseHaltonProjectionJitterInViewMatrices,
							 int32_t ViewRectX, int32_t ViewRectY, int32_t ViewRectW, int32_t ViewRectH);
	};
}
