#pragma once
#include "math/math.h"

namespace Engine
{

	struct CameraComponentPrivate
	{
		math::Matrix4x4 View;
		math::Matrix4x4 PreviousView;
		math::Vector3  CameraPos{0.f,0.f,0.f};
		math::Matrix4x4 PrevProjMatrix;
		math::Matrix4x4 ProjMatrix;
		float FovVertical = math::MATH_PI / 4.f;
		float Near = 0.1f;
		float Far = 1000.f;
		float Aspect = 1.0f;
		float jitterX = 0.f;
		float jitterY = 0.f;
		float PrevjitterX = 0.f;
		float PrevjitterY = 0.f;
		math::Frustum Frustum;
		uint32_t FrameIndex = 0;
		uint32_t FrameIndexMod2 = 0;

		// Bumped on NotifyTemporalHistoryInvalidate() or large per-frame camera teleport (TAA / SSR history).
		uint32_t TemporalHistoryGeneration = 0;
		math::Vector3 TemporalHistoryLastPos{ 0.f, 0.f, 0.f };
		bool TemporalHistoryHasLastPos = false;
		/** Until first Tick finishes jitter: avoids PrevView=id vs CurrView=lookAt → bogus motion/TAA after scene reload. */
		bool bTemporalPrevMatricesValid = false;
	};
}