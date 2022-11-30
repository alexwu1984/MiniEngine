#pragma once
#include "math/vector4.h"

namespace math
{
	class Quaternion;

	class Matrix4x4
	{
	public:
		union
		{
			float m[16];
			struct
			{
				float _00, _01, _02, _03;
				float _10, _11, _12, _13;
				float _20, _21, _22, _23;
				float _30, _31, _32, _33;
			};
			Vector4 row[4];
			struct { Vector4 r0, r1, r2, r3; };
		};

		Matrix4x4();
		Matrix4x4(float m00, float m01, float m02, float m03,
			float m10, float m11, float m12, float m13,
			float m20, float m21, float m22, float m23,
			float m30, float m31, float m32, float m33);
		Matrix4x4(const Vector4& r0, const Vector4& r1, const Vector4& r2, const Vector4& r3);
		Matrix4x4(Matrix4x4&& r);
		Matrix4x4(const Matrix4x4& r);
		~Matrix4x4();

		void Identity(void)
		{
			memset(m, 0, sizeof(Matrix4x4));
			_00 = _11 = _22 = _33 = 1.0f;
		}

		// Get the translation component of the matrix
		Vector3 GetTranslation() const
		{
			return Vector3(row[3].x, row[3].y, row[3].z);
		}

		// Get the X axis of the matrix (forward)
		Vector3 GetXAxis() const
		{
			Vector3 tmp(_00, _01, _02);
			tmp.Normalize();
			return tmp;
		}

		// Get the Y axis of the matrix (left)
		Vector3 GetYAxis() const
		{
			Vector3 tmp(_10, _11, _12);
			tmp.Normalize();
			return tmp;
		}

		// Get the Z axis of the matrix (up)
		Vector3 GetZAxis() const
		{
			Vector3 tmp(_20, _21, _22);
			tmp.Normalize();
			return tmp;
		}

		Matrix4x4 operator=(Matrix4x4&& r)
		{
			memcpy(m, r.m, sizeof(m));
			return *this;
		}

		Matrix4x4 operator=(const Matrix4x4& r)
		{
			memcpy(m, r.m, sizeof(m));
			return *this;
		}

		Vector4 Column(int i) const
		{
			return Vector4(r0[i], r1[i], r2[i], r3[i]);
		}

		Matrix4x4 operator *(const Matrix4x4& rhs) const;
		Matrix4x4& operator *= (const Matrix4x4& rhs);
		Matrix4x4 operator * (float rhs) const;
		Matrix4x4& operator *= (float rhs);

		Vector4 operator [](int index) const;
		Vector4& operator[](int index);


		Matrix4x4 Transpose() const;
		Matrix4x4 Inverse() const;

		void TransposeOf(const Matrix4x4& Matrix)
		{
			*this = Matrix.Transpose();
		}

		void InverseOf(const Matrix4x4& Matrix)
		{
			*this = Matrix.Inverse();
		}

		static Matrix4x4 CreateFromQuaternion(const Quaternion& q);

		Vector3 TranslateVector(const Vector3& vector) const;
		Vector3 TranslateVectorWithPrespDiv(const Vector3& vector)const;
		Vector3 TransformPosition(const Vector3& position)const;

		//∆Ω“∆æÿ’Û
		static Matrix4x4 CreateFromTranslate(float dx, float dy, float dz);
		static Matrix4x4 CreateFromTranslate(const Vector3& V);
		static Matrix4x4 ScaleMatrix(float s);
		static Matrix4x4 ScaleMatrix(const Vector3& T);
		static Matrix4x4 RotateX(float v);
		static Matrix4x4 RotateY(float v);
		static Matrix4x4 RotateZ(float v);
		static Matrix4x4 MatrixRotationRollPitchYaw(float Roll, float Pitch, float Yaw);
		static Matrix4x4 MatrixLookAtLH(const Vector3& EyePosition, const Vector3& FocusPosition, const Vector3& UpDirection);
		static Matrix4x4 MatrixPerspectiveFovLH(float FovAngleY, float AspectHByW, float NearZ, float FarZ);
		static Matrix4x4 MatrixOrthoLH(float Width, float Height, float NearZ, float FarZ);
		static Matrix4x4 MatrixOrthographicOffCenterLH(float ViewLeft,
													float ViewRight,
													float ViewBottom,
													float ViewTop,
													float NearZ,
													float FarZ);

		const static Matrix4x4 ms_Materix3X3WIdentity;
	};
}
