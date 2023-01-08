#pragma once
#include "math/vector3.h"

/*
AABB轴对称边界盒，3个中心轴A1,A2,A3永远平行于当前所在坐标轴，中心轴为单位坐标轴，fA1,fA2,fA3为半轴长度，所有内部的点都可以表示成
a * A1 + b * A2 + c * A3 a,b,c为参数，并且|a|<=fA1,|b|<=fA2,|c|<=fA3
https://zhuanlan.zhihu.com/p/35321344
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

		//通过最大点和最小点构造AABB
		AABB3(const Vector3& Max, const Vector3& Min);
		//通过中心点和3个轴的半长度构造AABB
		AABB3(const Vector3& Center, float fA0, float fA1, float fA2);
		AABB3(const Vector3& Center, float fA[3]);
		//通过点集合构造AABB
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

		//得到AABB8个点
		void GetPoint(Vector3 Point[8])const;
		//给定AABB内一点返回AABB的参数
		bool GetParameter(const Vector3& Point, float fAABBParameter[3])const;

		//用矩阵变换AABB project
		AABB3 Transform(const Matrix4x4& matrix) const;
		//合并2个AABB
		AABB3 MergeAABB(const AABB3& AABB)const;
		AABB3 GetMin(const AABB3& AABB) const;

		void UpdateMinMax(const Vector3& point);

		//AABB和AABB位置关系
//VSNOINTERSECT VSINTERSECT
		Intersect RelationWith(const AABB3& AABB)const;
		//点和AABB位置关系
//VSIN VSOUT VSON
		Intersect RelationWith(const Vector3& Point)const;

		//测试射线与AABB位置关系
		//VSNOINTERSECT VSNTERSECT
		Intersect RelationWith(const Ray3& Ray, float& tNear, float& tFar)const;
		//测试平面与AABB位置关系
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
		static const Vector3 _A[3]; //3个轴
		Vector3 _Center;			//中心点
		float	_fA[3];             //3个半轴的长度
		Vector3 _Max;				//最大点
		Vector3 _Min;				//最小点

	};
}