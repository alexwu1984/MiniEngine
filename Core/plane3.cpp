#include "math/plane3.h"
#include "math/ray3.h"
#include "math/aabb3.h"

namespace math 
{

	Plane3::Plane3(const Vector3& N, const Vector3& P)
	{
		Set(N, P);
	}

	Plane3::Plane3(const Vector3& P0, const Vector3& P1, const Vector3& P2)
	{
		Set(P0, P1, P2);
	}

	Plane3::Plane3(const Vector3 Point[3])
	{
		Set(Point);
	}

	Plane3::Plane3(const Vector3& N, float fD)
	{
		Set(N, fD);
	}

	Plane3::~Plane3()
	{

	}

	Intersect Plane3::RelationWith(const Vector3& Point) const
	{
		float f = Point.Dot(_N) + _fD;
		if (f > EPSILON_E4)
		{
			return Intersect::E_Front;
		}
		else if (f < -EPSILON_E4)
		{
			return Intersect::E_Back;
		}
		return Intersect::E_On;
	}

	Intersect Plane3::RelationWith(const Ray3& Ray, bool bCull, float& fRayParameter) const
	{
		return Ray.RelationWith(*this, bCull, fRayParameter);
	}

	Intersect Plane3::RelationWith(const AABB3& AABB) const
	{
		return AABB.RelationWith(*this);
	}

	Intersect Plane3::RelationWith(const Plane3& Plane) const
	{
		Vector3 vcCross;
		float     fSqrLength;

		vcCross.Cross(_N, Plane._N);
		fSqrLength = vcCross.GetSqrLength();


		if (fSqrLength < EPSILON_E4)
		{
			//return Plane.m_Point.RelationWith(*this);
			return Intersect::E_NoIntersect;
		}
		return Intersect::E_Intersect;
	}

}