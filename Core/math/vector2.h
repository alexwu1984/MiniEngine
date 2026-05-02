#pragma once
#include "core/inc.h"

namespace math
{
	class Vector2
	{
	public:
		union
		{
			float m[2];
			struct
			{
				float x;
				float y;
			};
		};
		Vector2() {}
		Vector2(float _x, float _y) :x(_x), y(_y) {}

		void Set(float _x, float _y)
		{
			x = _x; y = _y;
		}

		void operator += (const Vector2& v)
		{
			x += v.x;
			y += v.y;
		}
		void operator -= (const Vector2& v)
		{
			x -= v.x;
			y -= v.y;
		}

		void operator *= (float f)
		{
			x *= f;
			y *= f;
		}
		void operator /= (float f)
		{
			x /= f;
			y /= f;
		}
		void operator += (float f)
		{
			x += f;
			y += f;
		}
		void operator -= (float f)
		{
			x -= f;
			y -= f;
		}

		float     operator * (const Vector2& v)const
		{
			return (v.x * x + v.y * y);
		}

		bool operator ==(const Vector2& v)const
		{
			for (unsigned int i = 0; i < 2; i++)
			{
				if (std::abs(m[i] - v.m[i]) > EPSILON_E4)
				{
					return false;
				}
			}
			return true;
		}

		Vector2 operator * (float f)const
		{
			return Vector2(x * f, y * f);
		}

		Vector2 operator / (float f)const
		{
			return Vector2(x / f, y / f);
		}

		Vector2 operator + (float f)const
		{
			return Vector2(x + f, y + f);
		}

		Vector2 operator - (float f)const
		{
			return Vector2(x - f, y - f);
		}

		Vector2 operator + (const Vector2& v)const
		{
			return Vector2(x + v.x, y + v.y);
		}

		Vector2 operator - (const Vector2& v)const
		{
			return Vector2(x - v.x, y - v.y);
		}

		void Normalize();
	};

	inline bool Vector2Intersect(Vector2& A1, Vector2& B1, Vector2& A2, Vector2& B2, Vector2& Out)
	{
		float denominator = (B1.y * (A2.x - B2.x) + A1.y * (B2.x - A2.x) + (A1.x - B1.x) * (A2.y - B2.y));

		if (std::fabs(denominator) < 0.0001)
			return false;

		Out = Vector2((-B1.x * A2.y * B2.x + A1.y * B1.x * (B2.x - A2.x) + B1.x * A2.x * B2.y + A1.x * (B1.y * A2.x - B2.y * A2.x - B1.y * B2.x + A2.y * B2.x)) / denominator,
			(B1.y * (-A2.y * B2.x + A1.x * (A2.y - B2.y) + A2.x * B2.y) + A1.y * (A2.y * B2.x - A2.x * B2.y + B1.x * (B2.y - A2.y))) / denominator);

		return true;
	}
}