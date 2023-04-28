#pragma once
#include "math/vector3.h"

namespace math
{
	class Matrix4x4;

	class Vector4
	{
	public:
		union
		{
			float m[4];
			struct
			{
				float x, y, z, w;
			};
			struct
			{
				float r, g, b, a;
			};
		};


		Vector4(void);
		Vector4(float _x, float _y, float _z, float _w = 1.0f);
		Vector4(const Vector3& V,float _w = 0);

		float Dot(const Vector4& rhs) const
		{
			return x * rhs.x + y * rhs.y + z * rhs.z + w * rhs.w;
		}

		float& operator [] (int i)
		{
			return m[i];
		}
		const float& operator [] (int i) const
		{
			return m[i];
		}

		Vector4& operator += (const Vector4& rhs)
		{
			x += rhs.x;
			y += rhs.y;
			z += rhs.z;
			w += rhs.w;
			return *this;
		}

		Vector4 operator + (const Vector4& rhs) const
		{
			Vector4 Res(*this);
			Res += rhs;
			return Res;
		}

		Vector4 operator/(const float& other) const
		{
			return Vector4(this->x / other, this->y / other, this->z / other, this->w / other);
		}

		Vector4 operator - (const Vector4& v)const
		{
			return Vector4(x - v.x, y - v.y, z - v.z,w - v.w);
		}

		Vector4 operator*(const Matrix4x4& mat) const;

		float GetLength(void)const                  // 长度
		{
			return std::sqrt(x * x + y * y + z * z + w*w);
		}

		float GetSqrLength(void) const         // 长度平方
		{
			return (x * x + y * y + z * z + w*w);
		}
	};

	Vector4 operator * (const Vector4& lhs, float s);
	Vector4 operator * (float s, const Vector4& lhs);
	

}