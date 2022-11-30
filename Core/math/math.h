#pragma once
#define EPSILON_E4 (float)(1E-4)

namespace math
{
	enum class Intersect
	{
		E_Intersect,
		E_NoIntersect,
		E_Out,
		E_In,
		E_Front,
		E_Back,
		E_On,//点在面里面
	};

	template<typename T>
	inline T Clamp(const T& x, const T& low, const T& high)
	{
		return x < low ? low : (x > high ? high : x);
	}

	inline float Lerp(float a, float b, float f)
	{
		return a + f * (b - a);
	}

	const float MATH_PI = 3.141592654f;
	const float MATH_2PI = 2.f * MATH_PI;
	const float MATH_PI_HALF = 0.5f * MATH_PI;
	const float MATH_PI_OVER2 = MATH_PI / 2.0f;
	const float MATH_INFINITY = std::numeric_limits<float>::infinity();
	const float MATH_NEG_INFINITY = -std::numeric_limits<float>::infinity();
}