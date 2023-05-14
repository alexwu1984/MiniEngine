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

		float cX, cY, cZ, sX, sY, sZ, cXcZ, sXsZ, cXsZ, sXcZ;

		Pitch *= 0.5f;
		Yaw *= 0.5f;
		Roll *= 0.5f;

		cX = math::Cos(Pitch);
		cY = math::Cos(Yaw);
		cZ = math::Cos(Roll);

		sX = math::Sin(Pitch);
		sY = math::Sin(Yaw);
		sZ = math::Sin(Roll);

		cXcZ = cX * cZ;
		sXsZ = sX * sZ;
		cXsZ = cX * sZ;
		sXcZ = sX * cZ;

		Quaternion RotationQuat{};
		RotationQuat.w = cXcZ * cY + sXsZ * sY;
		RotationQuat.x = sXcZ * cY - cXsZ * sY;
		RotationQuat.y = cXcZ * sY + sXsZ * cY;
		RotationQuat.z = cXsZ * cY - sXcZ * sY;

		return RotationQuat;

		//Pitch *= 0.5f;
		//Yaw *= 0.5f;
		//Roll *= 0.5f;

		//float sx = math::Sin(Pitch);
		//float sy = math::Sin(Yaw);
		//float sz = math::Sin(Roll);
		//float cx = math::Cos(Pitch);
		//float cy = math::Cos(Yaw);
		//float cz = math::Cos(Roll);

		//Quaternion RotationQuat{};
		//RotationQuat.w = cx * cy * cz + sx * sy * sz;
		//RotationQuat.x = sx * cy * cz - cx * sy * sz;
		//RotationQuat.y = cx * sy * cz + sx * cy * sz;
		//RotationQuat.z = cx * cy * sz - sx * sy * cz;
		//return RotationQuat;

		//const float DEG_TO_RAD = math::MATH_PI / (180.f);
		//const float RADS_DIVIDED_BY_2 = DEG_TO_RAD / 2.f;
		//float SP, SY, SR;
		//float CP, CY, CR;

		//const float PitchNoWinding = math::Fmod(Pitch, 360.0f);
		//const float YawNoWinding = math::Fmod(Yaw, 360.0f);
		//const float RollNoWinding = math::Fmod(Roll, 360.0f);

		//math::SinCos(&SP, &CP, PitchNoWinding * RADS_DIVIDED_BY_2);
		//math::SinCos(&SY, &CY, YawNoWinding * RADS_DIVIDED_BY_2);
		//math::SinCos(&SR, &CR, RollNoWinding * RADS_DIVIDED_BY_2);

		//Quaternion RotationQuat{};
		//RotationQuat.x = CR * SP * SY - SR * CP * CY;
		//RotationQuat.y = -CR * SP * CY - SR * CP * SY;
		//RotationQuat.z = CR * CP * SY - SR * SP * CY;
		//RotationQuat.w = CR * CP * CY + SR * SP * SY;

		//return RotationQuat;
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