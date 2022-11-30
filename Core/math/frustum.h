#pragma once
#include "math/plane3.h"
#include "math/aabb3.h"

namespace math
{
	class Frustum
	{
	public:
		bool Intersects(const AABB3& box) const;

		// check intersect with point (world space)
		bool Intersects(const Vector3& p0) const;

		// check intersect with line segment (world space)
		bool Intersects(const Ray3& l) const;
	public:
		/**
		 * planes[0]: near;
		 * planes[1]: far;
		 * planes[2]: top;
		 * planes[3]: bottom;
		 * planes[4]: left;
		 * planes[5]: right;
		 */
		Plane3 planes[6];

		/**
		 * corners[0]: nearTopLeft;
		 * corners[1]: nearTopRight;
		 * corners[2]: nearBottomLeft;
		 * corners[3]: nearBottomRight;
		 * corners[4]: farTopLeft;
		 * corners[5]: farTopRight;
		 * corners[6]: farBottomLeft;
		 * corners[7]: farBottomRight;
		 */
		Vector3 corners[8];

		AABB3 bbox;
	};
}