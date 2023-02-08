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

		// Normalize the provided quaternion
		static Quaternion Normalize(const Quaternion& q)
		{
			Quaternion retVal = q;
			retVal.Normalize();
			return retVal;
		}

		//FROM UE4
		///** Rotation around the right axis (around Y axis), Looking up and down (0=Straight Ahead, +Up, -Down) */
		//Pitch;

		///** Rotation around the up axis (around Z axis), Running in circles 0=East, +North, -South. */
		//Yaw;

		///** Rotation around the forward axis (around X axis), Tilting your head, 0=Straight, +Clockwise, -CCW. */
		//Roll;

		static Quaternion MakeFromEuler(const Vector3& EulerAngle)
		{
			float Yaw = EulerAngle.z;
			float Pitch = EulerAngle.y;
			float Roll = EulerAngle.x;

			const float DEG_TO_RAD = math::MATH_PI / (180.f);
			const float RADS_DIVIDED_BY_2 = DEG_TO_RAD / 2.f;
			float SP, SY, SR;
			float CP, CY, CR;

			const float PitchNoWinding = math::Fmod(Pitch, 360.0f);
			const float YawNoWinding = math::Fmod(Yaw, 360.0f);
			const float RollNoWinding = math::Fmod(Roll, 360.0f);

			math::SinCos(&SP, &CP, PitchNoWinding * RADS_DIVIDED_BY_2);
			math::SinCos(&SY, &CY, YawNoWinding * RADS_DIVIDED_BY_2);
			math::SinCos(&SR, &CR, RollNoWinding * RADS_DIVIDED_BY_2);
			
			Quaternion RotationQuat;
			RotationQuat.x = CR * SP * SY - SR * CP * CY;
			RotationQuat.y = -CR * SP * CY - SR * CP * SY;
			RotationQuat.z = CR * CP * SY - SR * SP * CY;
			RotationQuat.w = CR * CP * CY + SR * SP * SY;

			return RotationQuat;
		}

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