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

	virtual math::Vector3 illuminate(const math::Vector3& lightDir, const math::Vector3& N,bool inShadow) const = 0;
	math::Vector3 position;
	math::Vector3 intensity;
};