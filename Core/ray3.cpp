#include "math/ray3.h"
#include "math/aabb3.h"
#include "math/plane3.h"

namespace math
{
	Ray3::Ray3(const Vector3& Orig, const Vector3& Dir)
		:_Orig(Orig)
		, _Dir(Dir)
	{

	}

	bool Ray3::GetParameter(const Vector3& Point, float& fLineParameter) const
	{
		Vector3 Temp = Point - _Orig;
		if (Temp.IsParallel(_Dir))
		{
			fLineParameter = Temp.GetLength();
			return true;
		}
		else
		{
			return false;
		}	
	}

	float Ray3::SquaredDistance(const Vector3& Point, float& fRayParameter) const
	{
		//使用向量投影进行计算
		Vector3 Diff = Point - _Orig;
		fRayParameter = _Dir.Dot(Diff); //diff在 m_dir直线方向的投影标量
		Diff -= _Dir * fRayParameter; //m_dir方向的向量，用diff 跟向量相减，得到 点到直线的垂直向量
		return Diff.GetSqrLength();
	}

	Intersect Ray3::RelationWith(const AABB3& AABB, float& tNear, float& tFar) const
	{
		float t0, t1, tmp;
		tNear = -(std::numeric_limits<float>::max)();
		tFar = (std::numeric_limits<float>::max)();
		Vector3 Min = AABB.GetMinPoint();
		Vector3 Max = AABB.GetMaxPoint();

		for (int i = 0; i < 3; i++)
		{
			if (std::abs(_Dir.m[i]) < EPSILON_E4)
			{
				if ((_Orig.m[i] < Min.m[i]) ||
					(_Orig.m[i] > Max.m[i]))
					return Intersect::E_NoIntersect;
			}
			t0 = (Min.m[i] - _Orig.m[i]) / _Dir.m[i];
			t1 = (Max.m[i] - _Orig.m[i]) / _Dir.m[i];
			if (t0 > t1) { tmp = t0; t0 = t1; t1 = tmp; }
			if (t0 > tNear) tNear = t0;
			if (t1 < tFar)  tFar = t1;
			if (tNear > tFar) return Intersect::E_NoIntersect;
			if (tFar < 0) return Intersect::E_NoIntersect;
		}
		return Intersect::E_Intersect;
	}

	Intersect Ray3::RelationWith(const Plane3& Plane, bool bCull, float& fRayParameter) const
	{
		float Vd = Plane.GetN().Dot(_Dir);
		if (std::abs(Vd) < EPSILON_E4)
		{

			return _Orig.RelationWith(Plane);
		}

		if (bCull && (Vd > 0.0f))
			return Intersect::E_NoIntersect;

		float Vo = -((Plane.GetN().Dot(_Orig)) + Plane.GetfD());
		float _t = Vo / Vd;
		fRayParameter = _t;

		if (fRayParameter < 0.f)
		{
			return _Orig.RelationWith(Plane);
		}

		return Intersect::E_Intersect;
	}

}


