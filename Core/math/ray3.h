#include "math/vector3.h"


namespace math
{
	class AABB3;
	class Plane3;

	class Ray3
	{
	public:
		Ray3() = default;
		Ray3(const Vector3& Orig, const Vector3& Dir);
		~Ray3() {}

		bool GetParameter(const Vector3& Point, float& fLineParameter)const;
		/************************inline***************************************/
		void Set(const Vector3& Orig, const Vector3& Dir)
		{
			_Orig = Orig;
			_Dir = Dir;
			_Dir.Normalize();
		}
		const Vector3& GetOrig()const
		{
			return _Dir;
		}
		const Vector3& GetDir()const
		{
			return _Dir;
		}

		Vector3 GetParameterPoint(float fRayParameter)const
		{
			if (fRayParameter < 0)
				fRayParameter = 0;
			return  (_Orig + _Dir * fRayParameter);
		}

		//点到直线距离
		float SquaredDistance(const Vector3& Point, float& fRayParameter) const;

		Intersect RelationWith(const AABB3& AABB, float& tNear, float& tFar) const;
		Intersect RelationWith(const Plane3& Plane, bool bCull, float& fRayParameter) const;

	protected:
		Vector3	_Orig;  // 源点
		Vector3	_Dir;   // 方向
	};
}