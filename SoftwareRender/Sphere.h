#pragma once
#include "Object.h"

class Sphere : public Object
{
public:
	Sphere(const math::Vector3& c, const float& r)
		: center(c)
		, radius(r)
		, radius2(r* r)
	{}

	bool intersect(const math::Vector3& orig, const math::Vector3& dir, float& tnear, uint32_t&, math::Vector2&) const override
	{
		//RayTracingInOneWeekend.html 5.1
		math::Vector3 L = orig - center;
		float a = math::Vector3::Dot(dir, dir);
		float b = 2 * math::Vector3::Dot(dir, L);
		float c = math::Vector3::Dot(L, L) - radius2;
		float t0, t1;
		if (!math::SolveQuadratic(a, b, c, t0, t1))
			return false;
		if (t0 < 0)
			t0 = t1;
		if (t0 < 0)
			return false;
		tnear = t0;
		return true;
	}

	void getSurfaceProperties(const math::Vector3& P, const math::Vector3&, const uint32_t&, const math::Vector2&, math::Vector3& N,
		math::Vector2&) const override
	{
		N = math::Vector3(P - center).Normalize();
	}

	math::Vector3 center;
	float radius = 0.f, radius2 = 0.f;
};