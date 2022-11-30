#include "math/quaternion.h"
#include "math/math.h"

namespace math 
{
	const Quaternion Quaternion::Identity(0.0f, 0.0f, 0.0f, 1.0f);

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