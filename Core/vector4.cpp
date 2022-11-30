#include "math/matrix4x4.h"

namespace math
{
	Vector4::Vector4(void)
		:x(0), y(0), z(0), w(0)
	{

	}

	Vector4::Vector4(float _x, float _y, float _z, float _w /*= 1.0f*/)
		: x(_x), y(_y), z(_z), w(_w)
	{

	}

	Vector4::Vector4(const Vector3& V, float _w /*= 0*/)
		: x(V.x), y(V.y), z(V.z), w(_w)
	{

	}

	Vector4 Vector4::operator*(const Matrix4x4& mat) const
	{
		float d0 = Dot(mat.Column(0));
		float d1 = Dot(mat.Column(1));
		float d2 = Dot(mat.Column(2));
		float d3 = Dot(mat.Column(3));
		return Vector4(d0, d1, d2, d3);
	}

	Vector4 operator * (const Vector4& lhs, float s)
	{
		return Vector4(lhs.x * s, lhs.y * s, lhs.z * s, lhs.w * s);
	}

	Vector4 operator * (float s, const Vector4& lhs)
	{
		return Vector4(lhs.x * s, lhs.y * s, lhs.z * s, lhs.w * s);
	}


}