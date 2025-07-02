#pragma once
#include "light.h"

class DirectionalLight : public Light
{
public:
	DirectionalLight(const math::Vector3& p, const math::Vector3& i)
		:Light(p, i)
	{
	}
	virtual ~DirectionalLight() = default;

	math::Vector3 illuminate(const math::Vector3& lightDir, const math::Vector3& N, bool inShadow) const override;
	math::Vector3 position;
	math::Vector3 intensity;
};