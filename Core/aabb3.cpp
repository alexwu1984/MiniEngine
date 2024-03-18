#include "math/aabb3.h"
#include "math/ray3.h"
#include "math/plane3.h"
#include "math/matrix4x4.h"

namespace math
{
	const Vector3 AABB3::_A[3] = { Vector3(1.0f,0.0f,0.0f),Vector3(0.0f,1.0f,0.0f),Vector3(0.0f,0.0f,1.0f) };
	AABB3::~AABB3()
	{

	}

	AABB3::AABB3(const Vector3& Max, const Vector3& Min)
	{
		Set(Max, Min);
	}

	AABB3::AABB3(const Vector3& Center, float fA0, float fA1, float fA2)
	{
		Set(Center, fA0, fA1, fA2);
	}

	AABB3::AABB3(const Vector3& Center, float fA[3])
	{
		Set(Center, fA);
	}

	void AABB3::CreateAABB(const std::vector<Vector3>& points)
	{
		_Min = points[0];
		_Max = points[0];
		for (unsigned int i = 1; i < points.size(); i++)
		{
			if (points[i].x < _Min.x)
			{
				_Min.x = points[i].x;
			}
			else if (points[i].x > _Max.x)
			{
				_Max.x = points[i].x;
			}

			if (points[i].y < _Min.y)
			{
				_Min.y = points[i].y;
			}
			else if (points[i].y > _Max.y)
			{
				_Max.y = points[i].y;
			}

			if (points[i].z < _Min.z)
			{
				_Min.z = points[i].z;
			}
			else if (points[i].z > _Max.z)
			{
				_Max.z = points[i].z;
			}
		}
		_Center = (_Min + _Max) / 2.0f;

		Vector3 Temp = (_Max - _Min) / 2.0f;

		_fA[0] = Temp.x;
		_fA[1] = Temp.y;
		_fA[2] = Temp.z;
	}

	void AABB3::GetPoint(Vector3 Point[8]) const
	{
		Point[0].Set(_Center.x + _fA[0], _Center.y + _fA[1], _Center.z + _fA[2]);
		Point[1].Set(_Center.x - _fA[0], _Center.y + _fA[1], _Center.z + _fA[2]);
		Point[2].Set(_Center.x - _fA[0], _Center.y - _fA[1], _Center.z + _fA[2]);
		Point[3].Set(_Center.x + _fA[0], _Center.y - _fA[1], _Center.z + _fA[2]);
		Point[4].Set(_Center.x + _fA[0], _Center.y + _fA[1], _Center.z - _fA[2]);
		Point[5].Set(_Center.x - _fA[0], _Center.y + _fA[1], _Center.z - _fA[2]);
		Point[6].Set(_Center.x - _fA[0], _Center.y - _fA[1], _Center.z - _fA[2]);
		Point[7].Set(_Center.x + _fA[0], _Center.y - _fA[1], _Center.z - _fA[2]);
	}

	bool AABB3::GetParameter(const Vector3& Point, float fAABBParameter[3]) const
	{
		for (int i = 0; i < 3; i++)
		{
			fAABBParameter[i] = Point.m[i] - _Center.m[i];
		}

		for (int i = 0; i < 3; i++)
		{
			if (std::abs(fAABBParameter[i]) > _fA[i])
			{
				return false;
			}		
		}

		return true;
	}


	AABB3 AABB3::Transform(const Matrix4x4& matrix) const
	{
		// Construct the 8 points for the corners of the box
		std::array<Vector3, 8> points;
		// Min point is always a corner
		points[0] = _Min;
		// Permutations with 2 min and 1 max
		points[1] = Vector3(_Max.x, _Min.y, _Min.z);
		points[2] = Vector3(_Min.x, _Max.y, _Min.z);
		points[3] = Vector3(_Min.x, _Min.y, _Max.z);
		// Permutations with 2 max and 1 min
		points[4] = Vector3(_Min.x, _Max.y, _Max.z);
		points[5] = Vector3(_Max.x, _Min.y, _Max.z);
		points[6] = Vector3(_Max.x, _Max.y, _Min.z);
		// Max point corner
		points[7] = Vector3(_Max);

		// Rotate first point
		//Vector3f p = Vector3f::Transform(points[0], q);
		Vector3 p = matrix.TranslateVectorWithPrespDiv(points[0]);
		// Reset min/max to first point rotated

		AABB3 NewAABB(p, p);
		// Update min/max based on remaining points, rotated
		for (size_t i = 1; i < points.size(); i++)
		{
			p = matrix.TranslateVectorWithPrespDiv(points[i]);
			NewAABB.UpdateMinMax(p);
		}
		return NewAABB;
	}

	AABB3 AABB3::MergeAABB(const AABB3& AABB) const
	{
		AABB3 Temp;
		Temp._Max = _Max;
		Temp._Min = _Min;
		if (Temp._Min.x > AABB._Min.x)
		{
			Temp._Min.x = AABB._Min.x;
		}
		if (Temp._Min.y > AABB._Min.y)
		{
			Temp._Min.y = AABB._Min.y;
		}
		if (Temp._Min.z > AABB._Min.z)
		{
			Temp._Min.z = AABB._Min.z;
		}

		if (Temp._Max.x < AABB._Max.x)
		{
			Temp._Max.x = AABB._Max.x;
		}
		if (Temp._Max.y < AABB._Max.y)
		{
			Temp._Max.y = AABB._Max.y;
		}
		if (Temp._Max.z < AABB._Max.z)
		{
			Temp._Max.z = AABB._Max.z;
		}
		Temp.Set(Temp._Max, Temp._Min);
		return Temp;
	}


	AABB3 AABB3::GetMin(const AABB3& AABB) const
	{
		AABB3 Temp;
		Temp._Max = _Max;
		Temp._Min = _Min;
		if (Temp._Min.x < AABB._Min.x)
		{
			Temp._Min.x = AABB._Min.x;
		}
		if (Temp._Min.y < AABB._Min.y)
		{
			Temp._Min.y = AABB._Min.y;
		}
		if (Temp._Min.z < AABB._Min.z)
		{
			Temp._Min.z = AABB._Min.z;
		}

		if (Temp._Max.x > AABB._Max.x)
		{
			Temp._Max.x = AABB._Max.x;
		}
		if (Temp._Max.y > AABB._Max.y)
		{
			Temp._Max.y = AABB._Max.y;
		}
		if (Temp._Max.z > AABB._Max.z)
		{
			Temp._Max.z = AABB._Max.z;
		}
		Temp.Set(Temp._Max, Temp._Min);
		return Temp;
	}

	void AABB3::UpdateMinMax(const Vector3& point)
	{
		// Update each component separately
		_Min.x = (std::min)(_Min.x, point.x);
		_Min.y = (std::min)(_Min.y, point.y);
		_Min.z = (std::min)(_Min.z, point.z);

		_Max.x = (std::max)(_Max.x, point.x);
		_Max.y = (std::max)(_Max.y, point.y);
		_Max.z = (std::max)(_Max.z, point.z);

		_Center = (_Min + _Max) / 2.0f;

		Vector3 Temp = (_Max - _Min) / 2.0f;

		_fA[0] = Temp.x;
		_fA[1] = Temp.y;
		_fA[2] = Temp.z;
	}

	math::Intersect AABB3::RelationWith(const AABB3& AABB) const
	{
		if ((_Min.x > AABB._Max.x) || (AABB._Min.x > _Max.x))
		{
			return math::Intersect::E_NoIntersect;
		}
		if ((_Min.y > AABB._Max.y) || (AABB._Min.y > _Max.y))
		{
			return math::Intersect::E_NoIntersect;
		}
		if ((_Min.z > AABB._Max.z) || (AABB._Min.z > _Max.z))
		{
			return math::Intersect::E_NoIntersect;
		}
			
		return math::Intersect::E_Intersect;
	}

	Intersect AABB3::RelationWith(const Vector3& Point) const
	{
		Vector3 PointTemp = Point - _Center;
		float fDiffX = std::abs(PointTemp.x) - _fA[0];
		float fDiffY = std::abs(PointTemp.y) - _fA[1];
		float fDiffZ = std::abs(PointTemp.z) - _fA[2];

		if (fDiffX > EPSILON_E4 || fDiffY > EPSILON_E4 || fDiffZ > EPSILON_E4)
			return Intersect::E_Out;

		if (fDiffX < -EPSILON_E4 || fDiffY < -EPSILON_E4 || fDiffZ < -EPSILON_E4)
			return Intersect::E_In;

		return Intersect::E_On;
	}

	Intersect AABB3::RelationWith(const Ray3& Ray, float& tNear, float& tFar) const
	{
		return Ray.RelationWith(*this, tNear, tFar);
	}

	Intersect AABB3::RelationWith(const Plane3& Plane) const
	{
		Vector3 N = Plane.GetN();
		float fD = Plane.GetfD();
		Vector3 MinTemp, MaxTemp;
		// x 
		if (N.x >= 0.0f)
		{
			MinTemp.x = _Min.x;
			MaxTemp.x = _Max.x;
		}
		else
		{
			MinTemp.x = _Max.x;
			MaxTemp.x = _Min.x;
		}
		// y 
		if (N.y >= 0.0f)
		{
			MinTemp.y = _Min.y;
			MaxTemp.y = _Max.y;
		}
		else
		{
			MinTemp.y = _Max.y;
			MaxTemp.y = _Min.y;
		}
		// z 
		if (N.z >= 0.0f)
		{
			MinTemp.z = _Min.z;
			MaxTemp.z = _Max.z;
		}
		else
		{
			MinTemp.z = _Max.z;
			MaxTemp.z = _Min.z;
		}

		if ((N.Dot(MinTemp) + fD) > 0.0f)
			return Intersect::E_Front;
		else if ((N.Dot(MaxTemp) + fD) < 0.0f)
			return Intersect::E_Back;
		else
			return Intersect::E_Intersect;
	}

	bool AABB3::GetIntersect(AABB3& AABB, AABB3& OutAABB) const
	{
		if (RelationWith(AABB) == math::Intersect::E_Intersect)
		{

			std::vector<float> XArray;
			XArray.push_back(GetMinPoint().x);
			XArray.push_back(GetMaxPoint().x);
			XArray.push_back(AABB.GetMinPoint().x);
			XArray.push_back(AABB.GetMaxPoint().x);

			std::sort(XArray.begin(), XArray.end());

			std::vector<float> YArray;
			YArray.push_back(GetMinPoint().y);
			YArray.push_back(GetMaxPoint().y);
			YArray.push_back(AABB.GetMinPoint().y);
			YArray.push_back(AABB.GetMaxPoint().y);

			std::vector<float> ZArray;
			ZArray.push_back(GetMinPoint().z);
			ZArray.push_back(GetMaxPoint().z);
			ZArray.push_back(AABB.GetMinPoint().z);
			ZArray.push_back(AABB.GetMaxPoint().z);

			OutAABB.Set(Vector3(XArray[2], YArray[2], ZArray[2]), Vector3(XArray[1], YArray[1], ZArray[1]));
			return true;

		}

		return false;
	}

}