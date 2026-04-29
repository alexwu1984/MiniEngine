#pragma once
#include "math/vector3.h"

namespace math
{
	class Ray3;
	class AABB3;

	class Plane3
	{
	public:
		Plane3() = default;
		Plane3(const Vector3& N, const Vector3& P);
		Plane3(const Vector3& P0, const Vector3& P1, const Vector3& P2);
		Plane3(const Vector3 Point[3]);
		Plane3(const Vector3& N, float fD);
		~Plane3();

		const Vector3& GetN()const { return _N; }
		Vector3 GetPoint()const
		{
			if (std::abs(_N.z) < EPSILON_E4)
			{
				return Vector3(0.0f, 0.0f, 0.0f);
			}
			return Vector3(0.0f, 0.0f, -_fD / _N.z);
		}
		float GetfD()const { return _fD; }
		void  Set(const Vector3& N, const Vector3& P)
		{
			_N = N;
			_N.Normalize();
			_fD = -(_N.Dot(P));
		}
		void  Set(const Vector3& N, float fD)
		{
			float Len = N.GetLength();
			_N = N / Len;
			_fD = fD / Len;
		}

		void  Set(const Vector3& P0, const Vector3& P1, const Vector3& P2)
		{
			Vector3 vcEdge1 = P1 - P0;
			Vector3 vcEdge2 = P2 - P0;

			_N.Cross(vcEdge1, vcEdge2);
			_N.Normalize();
			_fD = -(_N.Dot(P0));
		}
		void  Set(const Vector3 Point[3])
		{
			Set(Point[0], Point[1], Point[2]);
		}
		Plane3 GetPlane()const {
			return *this;
		}
		Vector3 ReflectDir(const Vector3& Dir)const
		{
			Vector3 TempN = _N * (-1.0f);

			return Dir - _N * Dir.Dot(_N) * 2.0f;
		}
		void  GetReverse(Plane3& OutPlane)const
		{
			OutPlane.Set(_N * (-1.0f), -_fD);
		}

		Intersect RelationWith(const Vector3& Point)const;
		Intersect RelationWith(const Ray3& Ray, bool bCull, float& fRayParameter)const;
		Intersect RelationWith(const AABB3& AABB)const;
		Intersect RelationWith(const Plane3& Plane)const;

	private:
		Vector3	_N;       // plane normal (not necessarily unit)
		float  _fD;       // signed distance-related term in ax+by+cz+d=0
	};
}
