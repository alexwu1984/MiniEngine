#include "math/quaternion.h"
#include "math/math.h"

namespace math 
{
	const Quaternion Quaternion::Identity(0.0f, 0.0f, 0.0f, 1.0f);

	void Quaternion::GetEulers(float& Pitch, float& Yaw, float& Roll)
	{
		float wx, wy, wz, xx, yy, yz, xy, xz, zz, x2, y2, z2;


		x2 = x + x;
		y2 = y + y;
		z2 = z + z;

		xx = x * x2;
		xy = x * y2;
		xz = x * z2;

		yy = y * y2;
		yz = y * z2;
		zz = z * z2;

		wx = w * x2;
		wy = w * y2;
		wz = w * z2;

		float _00 = 1.0f - (yy + zz);//
		float _01 = xy + wz;//


		float _10 = xy - wz;//
		float _11 = 1.0f - (xx + zz);//


		float _20 = xz + wy;//
		float _21 = yz - wx;//
		float _22 = 1.0f - (xx + yy);//

		if (_21 > 1.0f)
			Pitch = -math::MATH_2PI;
		else if (_21 < -1.0f)
			Pitch = math::MATH_2PI;
		else
			Pitch = math::Asin(-_21);

		if (std::fabs(_21) > 0.99999f)
		{
			Yaw = 0;
			Roll = math::Atan2(-_10, _00);

		}
		else
		{
			Roll = math::Atan2(_01, _11);
			Yaw = math::Atan2(_20, _22);

		}
	}

	Quaternion Quaternion::MakeFromEuler(float Pitch, float Yaw, float Roll)
	{

		Pitch *= 0.5f;
		Yaw *= 0.5f;
		Roll *= 0.5f;

		float cX = math::Cos(Pitch);
		float cY = math::Cos(Yaw);
		float cZ = math::Cos(Roll);

		float sX = math::Sin(Pitch);
		float sY = math::Sin(Yaw);
		float sZ = math::Sin(Roll);

		float cXcZ = cX * cZ;
		float sXsZ = sX * sZ;
		float cXsZ = cX * sZ;
		float sXcZ = sX * cZ;

		Quaternion RotationQuat{};
		RotationQuat.w = cXcZ * cY + sXsZ * sY;
		RotationQuat.x = sXcZ * cY - cXsZ * sY;
		RotationQuat.y = cXcZ * sY + sXsZ * cY;
		RotationQuat.z = cXsZ * cY - sXcZ * sY;

		return RotationQuat;
	}

	Quaternion Quaternion::FromToRotation(const Vector3& FromDirection, const Vector3& ToDirection)
	{
		auto TmpFromDirection = FromDirection.Normalize();
		auto TmpToDirection = ToDirection.Normalize();

		float DotValue = TmpFromDirection.Dot(TmpToDirection);
		if (math::Abs(DotValue - 1.0f) < DELTA)
		{
			return Quaternion(0.0f, 0.0f, 0.0f, 1.0f);
		}

		Vector3 Axis = Vector3::Cross(TmpFromDirection, TmpToDirection);
		float Angle = math::Acos(DotValue);
		return Quaternion(Axis.Normalize(), Angle);
	}

	Quaternion Quaternion::Lerp(const Quaternion& a, const Quaternion& b, float f)
	{
		Quaternion retVal;
		retVal.x = math::Lerp(a.x, b.x, f);
		retVal.y = math::Lerp(a.y, b.y, f);
		retVal.z = math::Lerp(a.z, b.z, f);
		retVal.w = math::Lerp(a.w, b.w, f);
		retVal.Normalize();
		return retVal;
	}

	
	Quaternion Quaternion::Slerp(const Quaternion& a, const Quaternion& b, float f)
	{
		float rawCosm = Quaternion::Dot(a, b);

		float cosom = -rawCosm;
		if (rawCosm >= 0.0f)
		{
			cosom = rawCosm;
		}

		float scale0, scale1;

		if (cosom < 0.9999f)
		{
			const float omega = std::acosf(cosom);
			const float invSin = 1.f / std::sin(omega);
			scale0 = std::sin((1.f - f) * omega) * invSin;
			scale1 = std::sin(f * omega) * invSin;
		}
		else
		{
			// Use linear interpolation if the quaternions
			// are collinear
			scale0 = 1.0f - f;
			scale1 = f;
		}

		if (rawCosm < 0.0f)
		{
			scale1 = -scale1;
		}

		Quaternion retVal;
		retVal.x = scale0 * a.x + scale1 * b.x;
		retVal.y = scale0 * a.y + scale1 * b.y;
		retVal.z = scale0 * a.z + scale1 * b.z;
		retVal.w = scale0 * a.w + scale1 * b.w;
		retVal.Normalize();
		return retVal;
	}

}