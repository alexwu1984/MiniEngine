#include "math/vector3.h"
#include "math/ray3.h"
#include "math/aabb3.h"
#include "math/plane3.h"
#include "math/matrix4x4.h"
#include "math/quaternion.h"

namespace math
{
	const Vector3 Vector3::Zero{ 0, 0, 0 };
	const Vector3 Vector3::UnitX{ 1.0f, 0.0f, 0.0f };
	const Vector3 Vector3::UnitY{ 0.f,1.f,0.f };
	const Vector3 Vector3::UnitZ{ 0.f,0.f,1.f };
	const Vector3 Vector3::NegUnitX{ -1.f,0.f,0.f };
	const Vector3 Vector3::NegUnitY{ 0.f,-1.f,0.f };
	const Vector3 Vector3::NegUnitZ{ 0.f,0.f,-1.f };
	const Vector3 Vector3::Infinity{ MATH_INFINITY, MATH_INFINITY, MATH_INFINITY };
	const Vector3 Vector3::NegInfinity{ MATH_NEG_INFINITY, MATH_NEG_INFINITY, MATH_NEG_INFINITY };

	Vector3::Vector3(float _x, float _y, float _z)
	{
		Set(_x, _y, _z);
	}

	Vector3::Vector3(const Vector3& v)
	{
		Set(v.x, v.y,v.z);
	}

	Vector3::Vector3()
	{
		Set(0.f, 0.f, 0.f);
	}

	Vector3 Vector3::operator * (const Matrix4x4& m)const
	{
		Vector3 vcResult;

		vcResult.x = x * m._00 + y * m._10 + z * m._20 + m._30;
		vcResult.y = x * m._01 + y * m._11 + z * m._21 + m._31;
		vcResult.z = x * m._02 + y * m._12 + z * m._22 + m._32;
		float w = x * m._03 + y * m._13 + z * m._23 + m._33;

		vcResult.x = vcResult.x / w;
		vcResult.y = vcResult.y / w;
		vcResult.z = vcResult.z / w;
		return vcResult;
	}

	Vector3 Vector3::Cross(const Vector3& v1, const Vector3& v2)
	{
		Vector3 Temp;
		Temp.x = v1.y * v2.z - v1.z * v2.y;
		Temp.y = v1.z * v2.x - v1.x * v2.z;
		Temp.z = v1.x * v2.y - v1.y * v2.x;
		return Temp;
	}


	Vector3 Vector3::Reflect(const Vector3& v, const Vector3& n)
	{
		return v - 2.0f * Dot(v, n) * n;
	}

	bool Vector3::IsParallel(const Vector3& Vector) const
	{
		float t0, t1;
		bool temp = false;
		t0 = x * Vector.y;
		t1 = y * Vector.x;

		if (std::abs(t0 - t1) > EPSILON_E4)
			temp = true;

		t0 = y * Vector.z;
		t1 = z * Vector.y;

		if (std::abs(t0 - t1) > EPSILON_E4 && temp)
			return true;
		else
			return false;
	}

	float Vector3::SquaredDistance(const Vector3& Point) const
	{
		return (x * Point.x + y * Point.y + z * Point.z);
	}


	float Vector3::SquaredDistance(const Ray3& Ray, float& fRayParameter) const
	{
		return Ray.SquaredDistance(*this, fRayParameter);
	}

	Intersect Vector3::RelationWith(const AABB3& AABB) const
	{
		return AABB.RelationWith(*this);
	}

	Intersect Vector3::RelationWith(const Plane3& Plane) const
	{
		return Plane.RelationWith(*this);
	}

	Vector3 Vector3::Transform(const Quaternion& q)
	{
		Vector3 qv(q.x, q.y, q.z);
		Vector3 retVal = *this;
		retVal += 2.0f * Vector3::Cross(qv, Vector3::Cross(qv, *this) + q.w * *this);
		return retVal;
	}

	Vector3 Vector3::Transform(const Vector3& v, const class Quaternion& q)
	{
		Vector3 qv(q.x, q.y, q.z);
		Vector3 retVal = v;
		retVal += 2.0f * Vector3::Cross(qv, Vector3::Cross(qv, v) + q.w * v);
		return retVal;
	}

	Vector3 operator * (float s, const Vector3& lhs)
	{
		return Vector3(lhs.x * s, lhs.y * s, lhs.z * s);
	}

}

