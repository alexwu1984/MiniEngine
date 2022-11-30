#include "math/frustum.h"

namespace math
{
	bool Frustum::Intersects(const AABB3& box) const
	{
		bool Cross = false;
		for (auto& plane : planes)
		{
			if (plane.RelationWith(box) == Intersect::E_Intersect)
			{
				Cross = true;
			}
		}

		if (Cross)
		{
			return true;
		}
		return bbox.RelationWith(box) == Intersect::E_Intersect;
	}

	bool Frustum::Intersects(const Vector3& p0) const
	{
		for (auto& plane : planes) {
			if (plane.RelationWith(p0) == Intersect::E_Back) {
				return false;
			}
		}

		return true;
	}

	bool Frustum::Intersects(const Ray3& l) const
	{
		float fRayParameter = 0.f;
		for (auto& plane : planes) {
			if (plane.RelationWith(l,false,fRayParameter) == Intersect::E_Back) {
				return false;
			}
		}

		return true;
	}

}