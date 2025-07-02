#pragma once
#include "light.h"

class PointPoint : public Light
{
public:
	PointPoint(const math::Vector3& p, const math::Vector3& i)
		:Light(p, i)
	{
	}
	virtual ~PointPoint() = default;

	math::Vector3 illuminate(const math::Vector3& lightDir, const math::Vector3& N, bool inShadow) const override;
};