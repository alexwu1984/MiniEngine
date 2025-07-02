#include "PointLight.h"


math::Vector3 PointPoint::illuminate(const math::Vector3& lightDir, const math::Vector3& N, bool inShadow) const
{
	float lightDistance2 = lightDir.GetSqrLength();
	float LdotN = std::max(0.f, math::Vector3::Dot(lightDir.Normalize(), N));
	if (inShadow)
		return math::Vector3(0, 0, 0); // No contribution if in shadow
	else
		return (intensity * LdotN / lightDistance2) ;
}
