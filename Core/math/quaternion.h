#pragma once
#include "math/vector4.h"

namespace math
{
	class Quaternion
	{
	public:
		float x;
		float y;
		float z;
		float w;

		Quaternion()
		{
			*this = Quaternion::Identity;
		}

		explicit Quaternion(const Vector4& InVec)
		{
			Set(InVec.x, InVec.y, InVec.z, InVec.w);
		}

		explicit Quaternion(float inX, float inY, float inZ, float inW)
		{
			Set(inX, inY, inZ, inW);
		}

		explicit Quaternion(const Vector3& axis, float angle)
		{
			float scalar = std::sin(angle / 2.0f);
			x = axis.x * scalar;
			y = axis.y * scalar;
			z = axis.z * scalar;
			w = std::cos(angle / 2.0f);
		}

		void Set(float inX, float inY, float inZ, float inW)
		{
			x = inX;
			y = inY;
			z = inZ;
			w = inW;
		}

		void Conjugate()
		{
			x *= -1.0f;
			y *= -1.0f;
			z *= -1.0f;
		}

		float LengthSq() const
		{
			return (x * x + y * y + z * z + w * w);
		}

		float Length() const
		{
			return std::sqrt(LengthSq());
		}

		void Normalize()
		{
			float length = Length();
			x /= length;
			y /= length;
			z /= length;
			w /= length;
		}

		void GetEulers(float& Pitch, float& Yaw, float& Roll);

		// Normalize the provided quaternion
		static Quaternion Normalize(const Quaternion& q)
		{
			Quaternion retVal = q;
			retVal.Normalize();
			return retVal;
		}

		static Quaternion MakeFromEuler(float Pitch, float Yaw, float Roll);
		// Creates a rotation which rotates from fromDirection to toDirection.
		static Quaternion FromToRotation(Vector3 FromDirection, Vector3 ToDirection);

		// Linear interpolation
		static Quaternion Lerp(const Quaternion& a, const Quaternion& b, float f);
		static float Dot(const Quaternion& a, const Quaternion& b)
		{
			return a.x * b.x + a.y * b.y + a.z * b.z + a.w * b.w;
		}
		static Quaternion Slerp(const Quaternion& a, const Quaternion& b, float f);

		// Concatenate
	// Rotate by q FOLLOWED BY p
		static Quaternion Concatenate(const Quaternion& q, const Quaternion& p)
		{
			Quaternion retVal;

			// Vector component is:
			// ps * qv + qs * pv + pv x qv
			Vector3 qv(q.x, q.y, q.z);
			Vector3 pv(p.x, p.y, p.z);
			Vector3 newVec = p.w * qv + q.w * pv + Vector3::Cross(pv, qv);
			retVal.x = newVec.x;
			retVal.y = newVec.y;
			retVal.z = newVec.z;

			// Scalar component is:
			// ps * qs - pv . qv
			retVal.w = p.w * q.w - Vector3::Dot(pv, qv);

			return retVal;
		}

		static const Quaternion Identity;
	};
}