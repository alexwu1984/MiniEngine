#pragma once
#include "math/vector3.h"

class Light
{
public:
	Light(const math::Vector3& p, const math::Vector3& i)
		: position(p)
		, intensity(i)
	{}
	virtual ~Light() = default;
	math::Vector3 position;
	math::Vector3 intensity;
};