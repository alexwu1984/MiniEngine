#pragma once
#include "math/math.h"

namespace Engine
{

	struct CameraComponentP
	{
		math::Matrix4x4 View;
		math::Vector3  CameraPos{0.f,0.f,0.f};
		math::Matrix4x4 ProjMatrix;
		float FovVertical = math::MATH_PI / 4.f;
		float Near = 0.01f;
		float Far = 1000.f;
		float Aspect = 1.0f;
		math::Frustum Frustum;
	};
}