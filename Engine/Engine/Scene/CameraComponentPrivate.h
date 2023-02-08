#pragma once
#include "math/math.h"

namespace Engine
{
	//FROM UE4
	///** Rotation around the right axis (around Y axis), Looking up and down (0=Straight Ahead, +Up, -Down) */
	//Pitch;

	///** Rotation around the up axis (around Z axis), Running in circles 0=East, +North, -South. */
	//Yaw;

	///** Rotation around the forward axis (around X axis), Tilting your head, 0=Straight, +Clockwise, -CCW. */
	//Roll;

	struct CameraComponentP
	{
		math::Matrix4x4 View;
		math::Vector3  CameraPos{-10.f,0.f,0.f};
		math::Matrix4x4 ProjMatrix;
		float FovVertical = math::MATH_PI / 4.f;
		float Near = 0.01f;
		float Far = 1000.f;
		float Aspect = 1.0f;
		math::Frustum Frustum;
	};
}