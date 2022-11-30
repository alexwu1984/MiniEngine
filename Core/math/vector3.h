#pragma once
#include "core/inc.h"

namespace math
{
	class Ray3;
	class AABB3;
	class Plane3;
	class Matrix4x4;
	class Quaternion;

	class Vector3
	{
	public:
		union
		{
			float m[3];
			struct
			{
				float x, y, z;
			};
		};


		Vector3() = default;
		Vector3(const Vector3& v);
		Vector3(float _x, float _y, float _z);

		void Set(float _x, float _y, float _z)
		{
			x = _x; _y = y; _z = z;
		}
		float GetLength(void)const                  // 长度
		{
			return std::sqrt(x * x + y * y + z * z);
		}

		float GetSqrLength(void) const         // 长度平方
		{
			return (x * x + y * y + z * z);
		}
		void  Negate(void)                    
		{
			x = -x; y = -y; z = -z;
		}
		void  Normalize(void)
		{
			float f = x * x + y * y + z * z;
			if (f > EPSILON_E4)
			{
				f = (float)1.0f / std::sqrt(f);
				x *= f; y *= f; z *= f;
			}
			else
			{
				*this = Zero;
			}
		}

		float Dot(const Vector3& v)const
		{
			return (v.x * x + v.y * y + v.z * z);
		}
		static Vector3 Cross(const Vector3& v1, const Vector3& v2);
		static float Dot(const Vector3& a, const Vector3& b)
		{
			return (a.x * b.x + a.y * b.y + a.z * b.z);
		}

		// a.b = |a|*|b|*cos(x)
		float AngleWith(const Vector3& v)          // 两个向量的夹角(弧度)
		{
			return std::acos(((*this).Dot(v)) / (this->GetLength() * v.GetLength()));
		}
		void  Create(const Vector3& v1,			// 构造向量从点v1到v2
			const Vector3& v2)
		{
			x = v2.x - v1.x;
			y = v2.y - v1.y;
			z = v2.z - v1.z;
		}

		/*            N     _
				 \	  /|\   /|
			Dir	  \	   |   /  Reflect
				  _\|  |  /
		--------------------------
		*/
		Vector3 Reflect(const Vector3& n)const
		{
			return n * Dot(n) * 2 + *this;
		}

		static Vector3 Reflect(const Vector3& v, const Vector3& n);

		void operator += (const Vector3& v)
		{
			x += v.x;   y += v.y;   z += v.z;
		}
		void operator -= (const Vector3& v)
		{
			x -= v.x;   y -= v.y;   z -= v.z;
		}

		void operator *= (const Vector3& v)
		{
			x *= v.x;   y *= v.y;   z *= v.z;
		}
		void operator /= (const Vector3& v)
		{
			x /= v.x;   y /= v.y;   z /= v.z;
		}

		void operator *= (float f)
		{
			x *= f;   y *= f;   z *= f;
		}
		void operator /= (float f)
		{
			x /= f;   y /= f;   z /= f;
		}
		void operator += (float f)
		{
			x += f;   y += f;   z += f;
		}
		void operator -= (float f)
		{
			x -= f;   y -= f;   z -= f;
		}


		Vector3 operator=(const Vector3& v)
		{
			Set(v.x, v.y, v.z);
			return *this;
		}

		Vector3 operator * (float f)const
		{
			return Vector3(x * f, y * f, z * f);
		}

		Vector3 operator * (Vector3 v)const
		{
			return Vector3(x * v.x, y * v.y, z * v.z);
		}

		Vector3 operator / (float f)const
		{
			return Vector3(x / f, y / f, z / f);
		}
		Vector3 operator + (float f)const
		{
			return Vector3(x + f, y + f, z + f);
		}
		Vector3 operator + (const Vector3& v)const
		{
			return Vector3(x + v.x, y + v.y, z + v.z);
		}
		Vector3 operator - (float f)const
		{
			return Vector3(x - f, y - f, z - f);
		}

		Vector3 operator - (const Vector3& v)const
		{
			return Vector3(x - v.x, y - v.y, z - v.z);
		}

		Vector3 operator * (const Matrix4x4& m)const;

		bool IsParallel(const Vector3& Vector)const;

		//点和点距离
		float SquaredDistance(const Vector3& Point)const;
		//点和射线距离
		float SquaredDistance(const Ray3& Ray, float& fRayParameter)const;

		//VSIN VSOUT VSON
		Intersect RelationWith(const AABB3& AABB)const;
		Intersect RelationWith(const Plane3& Plane)const;

		Vector3 Transform(const Quaternion& q);
		static Vector3 Transform(const Vector3& v, const Quaternion& q);
public:
		static const Vector3 Zero;
		static const Vector3 UnitX;
		static const Vector3 UnitY;
		static const Vector3 UnitZ;
		static const Vector3 NegUnitX;
		static const Vector3 NegUnitY;
		static const Vector3 NegUnitZ;
		static const Vector3 Infinity;
		static const Vector3 NegInfinity;
	};

	Vector3 operator * (float s, const Vector3& lhs);

}