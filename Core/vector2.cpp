#include "math/vector2.h"

void math::Vector2::Normalize()
{
	float f = x * x + y * y;
	if (f > EPSILON_E4)
	{
		f = (float)1.0f / std::sqrtf(f);
		x *= f; y *= f;
	}
	else
	{
		Set(0.0f, 0.0f);
	}
}
