#pragma once
#include "math/vector3.h"

/*
 * Axis-aligned bounding box (AABB): three center axes stay parallel to the current
 * coordinate axes (world X/Y/Z). Center C, half-extents fA0..fA2 along those axes;
 * any interior point is C + a*e0 + b*e1 + c*e2 with |a|<=fA0, |b|<=fA1, |c|<=fA2.
 * See also: https://zhuanlan.zhihu.com/p/35321344
 */
namespace math
{
	class Ray3;
	class Plane3;
	class Matrix4x4;

	class AABB3
	{
	public:
		AABB3() = default;
		~AABB3();

		// from min/max corners
		AABB3(const Vector3& Max, const Vector3& Min);
		// from center and positive half-extents along X/Y/Z
		AABB3(const Vector3& Center, float fA0, float fA1, float fA2);
		AABB3(const Vector3& Center, float fA[3]);
		// tight AABB around point set
		void CreateAABB(const std::vector<Vector3>& points);
		/*********************************** inline *************************************/

		void Set(const Vector3& Max, const Vector3& Min)
		{
			_Max = Max;
			_Min = Min;
			_Center = (Max + Min) / 2.0f;
			Vector3 Temp = (Max - Min) / 2.0f;
			for (int i = 0; i < 3; i++)
				_fA[i] = Temp.m[i];
		}

		void Set(const Vector3& Center, float fA0, float fA1, float fA2)
		{
			_fA[0] = std::abs(fA0);
			_fA[1] = std::abs(fA1);
			_fA[2] = std::abs(fA2);

			_Center = Center;

			_Max.Set(_Center.x + _fA[0], _Center.y + _fA[1], _Center.z + _fA[2]);
			_Min.Set(_Center.x - _fA[0], _Center.y - _fA[1], _Center.z - _fA[2]);
		}

		void Set(const Vector3& Center, float fA[3])
		{
			_fA[0] = std::abs(fA[0]);
			_fA[1] = std::abs(fA[1]);
			_fA[2] = std::abs(fA[2]);

			_Center = Center;

			_Max.Set(_Center.x + _fA[0], _Center.y + _fA[1], _Center.z + _fA[2]);
			_Min.Set(_Center.x - _fA[0], _Center.y - _fA[1], _Center.z - _fA[2]);
		}

		void GetfA(float fA[3])const
		{
			for (int i = 0; i < 3; i++)
			{
				fA[i] = _fA[i];
			}
		}
		const Vector3& GetCenter() const { return _Center; }
		Vector3 GetParameterPoint(float fAABBParameter[3])const
		{

			return Vector3(_Center.x + fAABBParameter[0], _Center.y + fAABBParameter[1], _Center.z + fAABBParameter[2]);
		}

		Vector3 GetParameterPoint(float fAABBParameter0, float fAABBParameter1, float fAABBParameter2)const
		{
			return Vector3(_Center.x + fAABBParameter0, _Center.y + fAABBParameter1, _Center.z + fAABBParameter2);
		}

		const Vector3& GetMaxPoint()const { return _Max; }
		const Vector3& GetMinPoint()const { return _Min; }

		// eight corner vertices
		void GetPoint(Vector3 Point[8])const;
		// barycentric-like parameters for a point inside (relative to center/axes)
		bool GetParameter(const Vector3& Point, float fAABBParameter[3])const;

		// transform corners and rebuild AABB (may grow)
		AABB3 Transform(const Matrix4x4& matrix) const;
		// union of two AABBs
		AABB3 MergeAABB(const AABB3& AABB)const;
		AABB3 GetMin(const AABB3& AABB) const;

		void UpdateMinMax(const Vector3& point);

		// AABB vs AABB (VSNOINTERSECT / VSINTERSECT)
		Intersect RelationWith(const AABB3& AABB)const;
		// point vs AABB (VSIN / VSOUT / VSON)
		Intersect RelationWith(const Vector3& Point)const;

		// ray vs AABB slab test (VSNOINTERSECT / VSINTERSECT)
		Intersect RelationWith(const Ray3& Ray, float& tNear, float& tFar)const;
		// plane vs AABB
		Intersect RelationWith(const Plane3& Plane)const;

		bool GetIntersect(AABB3& AABB, AABB3& OutAABB)const;

		float GetRadius() const
		{
			return std::sqrt(GetSqrRadius());
		}

		float GetSqrRadius() const
		{
			return 0.25f * (_Max - _Min).GetSqrLength();
		}

	private:
		static const Vector3 _A[3]; // world axis directions
		Vector3 _Center;			// box center
		float	_fA[3];             // half-extents along X/Y/Z
		Vector3 _Max;				// max corner
		Vector3 _Min;				// min corner

	};
}