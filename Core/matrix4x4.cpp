#include "math/matrix4x4.h"
#include "math/quaternion.h"

namespace math
{

	const Matrix4x4 Matrix4x4::ms_Materix3X3WIdentity;
	Matrix4x4::Matrix4x4()
	{
		Identity();
	}

	Matrix4x4::Matrix4x4(float m00, float m01, float m02, float m03, float m10, float m11, float m12, float m13, 
						float m20, float m21, float m22, float m23, float m30, float m31, float m32, float m33)
	{
		_00 = m00; _01 = m01; _02 = m02; _03 = m03;
		_10 = m10; _11 = m11; _12 = m12; _13 = m13;
		_20 = m20; _21 = m21; _22 = m22; _23 = m23;
		_30 = m30; _31 = m31; _32 = m32; _33 = m33;
	}


	Matrix4x4::Matrix4x4(const Matrix4x4& r)
	{
		memcpy(m, r.m, sizeof(m));
	}

	Matrix4x4::Matrix4x4(Matrix4x4&& r)
	{
		memcpy(m, r.m, sizeof(m));
		memset(r.m, 0, sizeof(r.m));
	}

	Matrix4x4::Matrix4x4(const Vector4& _r0, const Vector4& _r1, const Vector4& _r2, const Vector4& _r3)
		:r0(_r0), r1(_r1), r2(_r2), r3(_r3)
	{

	}

	Matrix4x4::~Matrix4x4()
	{

	}

	

	Matrix4x4 Matrix4x4::Transpose() const
	{
		return Matrix4x4(
			r0.x, r1.x, r2.x, r3.x,
			r0.y, r1.y, r2.y, r3.y,
			r0.z, r1.z, r2.z, r3.z,
			r0.w, r1.w, r2.w, r3.w
		);
	}

	Matrix4x4 Matrix4x4::Inverse() const
	{
		// https://semath.info/src/inverse-cofactor-ex4.html
		float det = r0.x * r1.y * r2.z * r3.w + r0.x * r1.z * r2.w * r3.y + r0.x * r1.w * r2.y * r3.z
			- r0.x * r1.w * r2.z * r3.y - r0.x * r1.z * r2.y * r3.w - r0.x * r1.y * r2.w * r3.z
			- r0.y * r1.x * r2.z * r3.w - r0.z * r1.x * r2.w * r3.y - r0.w * r1.x * r2.y * r3.z
			+ r0.w * r1.x * r2.z * r3.y + r0.z * r1.x * r2.y * r3.w + r0.y * r1.x * r2.w * r3.z
			+ r0.y * r1.z * r2.x * r3.w + r0.z * r1.w * r2.x * r3.y + r0.w * r1.y * r2.x * r3.z
			- r0.w * r1.z * r2.x * r3.y - r0.z * r1.y * r2.x * r3.w - r0.y * r1.w * r2.x * r3.z
			- r0.y * r1.z * r2.w * r3.x - r0.z * r1.w * r2.y * r3.x - r0.w * r1.y * r2.z * r3.x
			+ r0.w * r1.z * r2.y * r3.x + r0.z * r1.y * r2.w * r3.x + r0.y * r1.w * r2.z * r3.x;

		float A11 = r1.y * r2.z * r3.w + r1.z * r2.w * r3.y + r1.w * r2.y * r3.z - r1.w * r2.z * r3.y - r1.z * r2.y * r3.w - r1.y * r2.w * r3.z;
		float A12 = -r0.y * r2.z * r3.w - r0.z * r2.w * r3.y - r0.w * r2.y * r3.z + r0.w * r2.z * r3.y + r0.z * r2.y * r3.w + r0.y * r2.w * r3.z;
		float A13 = r0.y * r1.z * r3.w + r0.z * r1.w * r3.y + r0.w * r1.y * r3.z - r0.w * r1.z * r3.y - r0.z * r1.y * r3.w - r0.y * r1.w * r3.z;
		float A14 = -r0.y * r1.z * r2.w - r0.z * r1.w * r2.y - r0.w * r1.y * r2.z + r0.w * r1.z * r2.y + r0.z * r1.y * r2.w + r0.y * r1.w * r2.z;

		float A21 = -r1.x * r2.z * r3.w - r1.z * r2.w * r3.x - r1.w * r2.x * r3.z + r1.w * r2.z * r3.x + r1.z * r2.x * r3.w + r1.x * r2.w * r3.z;
		float A22 = r0.x * r2.z * r3.w + r0.z * r2.w * r3.x + r0.w * r2.x * r3.z - r0.w * r2.z * r3.x - r0.z * r2.x * r3.w - r0.x * r2.w * r3.z;
		float A23 = -r0.x * r1.z * r3.w - r0.z * r1.w * r3.x - r0.w * r1.x * r3.z + r0.w * r1.z * r3.x + r0.z * r1.x * r3.w + r0.x * r1.w * r3.z;
		float A24 = r0.x * r1.z * r2.w + r0.z * r1.w * r2.x + r0.w * r1.x * r2.z - r0.w * r1.z * r2.x - r0.z * r1.x * r2.w - r0.x * r1.w * r2.z;

		float A31 = r1.x * r2.y * r3.w + r1.y * r2.w * r3.x + r1.w * r2.x * r3.y - r1.w * r2.y * r3.x - r1.y * r2.x * r3.w - r1.x * r2.w * r3.y;
		float A32 = -r0.x * r2.y * r3.w - r0.y * r2.w * r3.x - r0.w * r2.x * r3.y + r0.w * r2.y * r3.x + r0.y * r2.x * r3.w + r0.x * r2.w * r3.y;
		float A33 = r0.x * r1.y * r3.w + r0.y * r1.w * r3.x + r0.w * r1.x * r3.y - r0.w * r1.y * r3.x - r0.y * r1.x * r3.w - r0.x * r1.w * r3.y;
		float A34 = -r0.x * r1.y * r2.w - r0.y * r1.w * r2.x - r0.w * r1.x * r2.y + r0.w * r1.y * r2.x + r0.y * r1.x * r2.w + r0.x * r1.w * r2.y;

		float A41 = -r1.x * r2.y * r3.z - r1.y * r2.z * r3.x - r1.z * r2.x * r3.y + r1.z * r2.y * r3.x + r1.y * r2.x * r3.z + r1.x * r2.z * r3.y;
		float A42 = r0.x * r2.y * r3.z + r0.y * r2.z * r3.x + r0.z * r2.x * r3.y - r0.z * r2.y * r3.x - r0.y * r2.x * r3.z - r0.x * r2.z * r3.y;
		float A43 = -r0.x * r1.y * r3.z - r0.y * r1.z * r3.x - r0.z * r1.x * r3.y + r0.z * r1.y * r3.x + r0.y * r1.x * r3.z + r0.x * r1.z * r3.y;
		float A44 = r0.x * r1.y * r2.z + r0.y * r1.z * r2.x + r0.z * r1.x * r2.y - r0.z * r1.y * r2.x - r0.y * r1.x * r2.z - r0.x * r1.z * r2.y;

		return Matrix4x4(A11, A12, A13, A14, A21, A22, A23, A24, A31, A32, A33, A34, A41, A42, A43, A44) * (1.f / det);
	}

	Matrix4x4 Matrix4x4::operator*(const Matrix4x4& rhs) const
	{
		Vector4 c0 = rhs.Column(0);
		Vector4 c1 = rhs.Column(1);
		Vector4 c2 = rhs.Column(2);
		Vector4 c3 = rhs.Column(3);

		return Matrix4x4(
			r0.Dot(c0), r0.Dot(c1), r0.Dot(c2), r0.Dot(c3),
			r1.Dot(c0), r1.Dot(c1), r1.Dot(c2), r1.Dot(c3),
			r2.Dot(c0), r2.Dot(c1), r2.Dot(c2), r2.Dot(c3),
			r3.Dot(c0), r3.Dot(c1), r3.Dot(c2), r3.Dot(c3)
		);
	}

	Matrix4x4& Matrix4x4::operator *= (const Matrix4x4& rhs)
	{
		*this = (*this) * rhs;
		return *this;
	}

	Matrix4x4 Matrix4x4::operator * (float rhs) const
	{
		return Matrix4x4(r0 * rhs, r1 * rhs, r2 * rhs, r3 * rhs);
	}

	Matrix4x4& Matrix4x4::operator *= (float rhs)
	{
		*this = (*this) * rhs;
		return *this;
	}

	Vector4 Matrix4x4::operator [](int index) const
	{
		Assert(index < 4);
		return row[index];
	}
	Vector4& Matrix4x4::operator[](int index)
	{
		Assert(index < 4);
		return row[index];
	}

	Matrix4x4 Matrix4x4::CreateFromTranslate(float dx, float dy, float dz)
	{
		Matrix4x4 Temp;
		Temp.Identity();
		Temp._30 = dx;
		Temp._31 = dy;
		Temp._32 = dz;
		return Temp;
	}

	Matrix4x4 Matrix4x4::CreateFromTranslate(const Vector3& V)
	{
		Matrix4x4 Temp;
		Temp.Identity();
		Temp._30 = V.x;
		Temp._31 = V.y;
		Temp._32 = V.z;
		return Temp;
	}

	Matrix4x4 Matrix4x4::ScaleMatrix(float s)
	{
		return Matrix4x4(
			s, 0.f, 0.f, 0.f,
			0.f, s, 0.f, 0.f,
			0.f, 0.f, s, 0.f,
			0.f, 0.f, 0.f, 1.f);
	}

	Matrix4x4 Matrix4x4::ScaleMatrix(const Vector3& T)
	{
		return Matrix4x4(
			T.x, 0.f, 0.f, 0.f,
			0.f, T.y, 0.f, 0.f,
			0.f, 0.f, T.z, 0.f,
			0.f, 0.f, 0.f, 1.f);
	}

	Matrix4x4 Matrix4x4::RotateX(float v)
	{
		float c = (float)std::cos(v);
		float s = (float)std::sin(v);
		return Matrix4x4(
			1.f, 0.f, 0.f, 0.f,
			0.f, c, s, 0.f,
			0.f, -s, c, 0.f,
			0.f, 0.f, 0.f, 1.f);
	}

	Matrix4x4 Matrix4x4::RotateY(float v)
	{
		float c = (float)std::cos(v);
		float s = (float)std::sin(v);
		return Matrix4x4(
			c, 0.f, -s, 0.f,
			0.f, 1.f, 0.f, 0.f,
			s, 0.f, c, 0.f,
			0.f, 0.f, 0.f, 1.f);
	}

	Matrix4x4 Matrix4x4::RotateZ(float v)
	{
		float c = (float)std::cos(v);
		float s = (float)std::sin(v);
		return Matrix4x4(
			c, s, 0.f, 0.f,
			-s, c, 0.f, 0.f,
			0.f, 0.f, 1.f, 0.f,
			0.f, 0.f, 0.f, 1.f);
	}

	Matrix4x4 Matrix4x4::MatrixRotationRollPitchYaw(float Roll, float Pitch, float Yaw)
	{
		// roll(z), pitch(x), yaw(y)
		return Matrix4x4::RotateZ(Roll) * Matrix4x4::RotateX(Pitch) * Matrix4x4::RotateY(Yaw);
	}

	Matrix4x4 Matrix4x4::MatrixLookAtLH(const Vector3& EyePosition, const Vector3& FocusPosition, const Vector3& UpDirection)
	{
		Vector3 Forward = FocusPosition - EyePosition;
		Forward.Normalize();
		Vector3 Up = UpDirection;
		Up.Normalize();

		Vector3 Right = Vector3::Cross(Up, Forward);
		Up = Vector3::Cross(Forward, Right);

		float D0 = -EyePosition.Dot(Right);
		float D1 = -EyePosition.Dot(Up);
		float D2 = -EyePosition.Dot(Forward);

		Matrix4x4 Temp;
		Temp._00 = Right.x;
		Temp._10 = Right.y;
		Temp._20 = Right.z;
		Temp._30 = D0;

		Temp._01 = Up.x;
		Temp._11 = Up.y;
		Temp._21 = Up.z;
		Temp._31 = D1;

		Temp._02 = Forward.x;
		Temp._12 = Forward.y;
		Temp._22 = Forward.z;
		Temp._33 = D2;

		Temp._03 = 0.0f; Temp._13 = 0.0f; Temp._23 = 0.0f; Temp._33 = 1.0f;

		return Temp;
	}

	Matrix4x4 Matrix4x4::CreateFromQuaternion(const Quaternion& q)
	{
		Matrix4x4 mat;

		mat[0][0] = 1.0f - 2.0f * q.y * q.y - 2.0f * q.z * q.z;
		mat[0][1] = 2.0f * q.x * q.y + 2.0f * q.w * q.z;
		mat[0][2] = 2.0f * q.x * q.z - 2.0f * q.w * q.y;
		mat[0][3] = 0.0f;

		mat[1][0] = 2.0f * q.x * q.y - 2.0f * q.w * q.z;
		mat[1][1] = 1.0f - 2.0f * q.x * q.x - 2.0f * q.z * q.z;
		mat[1][2] = 2.0f * q.y * q.z + 2.0f * q.w * q.x;
		mat[1][3] = 0.0f;

		mat[2][0] = 2.0f * q.x * q.z + 2.0f * q.w * q.y;
		mat[2][1] = 2.0f * q.y * q.z - 2.0f * q.w * q.x;
		mat[2][2] = 1.0f - 2.0f * q.x * q.x - 2.0f * q.y * q.y;
		mat[2][3] = 0.0f;

		mat[3][0] = 0.0f;
		mat[3][1] = 0.0f;
		mat[3][2] = 0.0f;
		mat[3][3] = 1.0f;

		return mat;
	}

	Vector3 Matrix4x4::TranslateVector(const Vector3& vector) const
	{
		Vector4 Res = Vector4(vector, 0.f) * (*this);
		return Vector3(Res.x, Res.y, Res.z);
	}

	Vector3 Matrix4x4::TranslateVectorWithPrespDiv(const Vector3& vector) const
	{
		Vector4 Res = Vector4(vector, 1.f) * (*this);
		return Vector3(Res.x / Res.w, Res.y / Res.w, Res.z / Res.w);
	}

	Vector3 Matrix4x4::TransformPosition(const Vector3& position) const
	{
		Vector4 Res = Vector4(position, 1.f) * (*this);
		return Vector3(Res.x, Res.y, Res.z);
	}

	Matrix4x4 Matrix4x4::MatrixPerspectiveFovLH(float FovAngleY, float AspectHByW, float NearZ, float FarZ)
	{
		float h = 1.f / (float)std::tan(FovAngleY / 2);
		float w = h / AspectHByW;
		return Matrix4x4(
			w, 0.f, 0.f, 0.f,
			0.f, h, 0.f, 0.f,
			0.f, 0.f, FarZ / (FarZ - NearZ), 1,
			0.f, 0.f, -NearZ * FarZ / (FarZ - NearZ), 0.f
		);
	}

	Matrix4x4 Matrix4x4::MatrixOrthoLH(float Width, float Height, float NearZ, float FarZ)
	{
		float Dist = FarZ - NearZ;
		return Matrix4x4(
			2 / Width, 0, 0, 0,
			0, 2 / Height, 0, 0,
			0, 0, 1 / Dist, 0,
			0, 0, -NearZ / Dist, 1);
	}

	Matrix4x4 Matrix4x4::MatrixOrthographicOffCenterLH(float ViewLeft, float ViewRight, float ViewBottom, float ViewTop, float NearZ, float FarZ)
	{
		float ReciprocalWidth = 1.0f / (ViewRight - ViewLeft);
		float ReciprocalHeight = 1.0f / (ViewTop - ViewBottom);
		float fRange = 1.0f / (FarZ - NearZ);
		return Matrix4x4(
			ReciprocalWidth + ReciprocalWidth, 0, 0, 0,
			0, ReciprocalHeight + ReciprocalHeight, 0, 0,
			0, 0, fRange, 0,
			-(ViewLeft + ViewRight) * ReciprocalWidth, -(ViewTop + ViewBottom) * ReciprocalHeight, -fRange * NearZ, 1
		);
	}

}