#pragma once
#include "Object.h"
#include "math/ray3.h"

class MeshTriangle : public Object
{
public:
	MeshTriangle(const math::Vector3* verts, const uint32_t* vertsIndex, const uint32_t& numTris, const math::Vector2* st)
	{
		uint32_t maxIndex = 0;
		for (uint32_t i = 0; i < numTris * 3; ++i)
			if (vertsIndex[i] > maxIndex)
				maxIndex = vertsIndex[i];
		maxIndex += 1;
		vertices = std::unique_ptr < math::Vector3[] > (new math::Vector3[maxIndex]);
		memcpy(vertices.get(), verts, sizeof(math::Vector3) * maxIndex);
		vertexIndex = std::unique_ptr<uint32_t[]>(new uint32_t[numTris * 3]);
		memcpy(vertexIndex.get(), vertsIndex, sizeof(uint32_t) * numTris * 3);
		numTriangles = numTris;
		stCoordinates = std::unique_ptr<math::Vector2[]>(new math::Vector2[maxIndex]);
		memcpy(stCoordinates.get(), st, sizeof(math::Vector2) * maxIndex);
	}

	bool intersect(const math::Vector3& orig, const math::Vector3& dir, float& tnear, uint32_t& index,
		math::Vector2& uv) const override
	{
		bool intersect = false;
		for (uint32_t k = 0; k < numTriangles; ++k)
		{
			const math::Vector3& v0 = vertices[vertexIndex[k * 3]];
			const math::Vector3& v1 = vertices[vertexIndex[k * 3 + 1]];
			const math::Vector3& v2 = vertices[vertexIndex[k * 3 + 2]];
			math::Ray3 ray(orig, dir);
			math::HitResult hitRet = math::RayTriangleIntersect(v0, v1, v2, ray);
			if (hitRet.Hit && hitRet.tNear < tnear)
			{
				tnear = hitRet.tNear;
				uv.x = hitRet.u;
				uv.y = hitRet.v;
				index = k;
				intersect |= true;
			}
		}

		return intersect;
	}

	void getSurfaceProperties(const math::Vector3&, const math::Vector3&, const uint32_t& index, const math::Vector2& uv, math::Vector3& N,
		math::Vector2& st) const override
	{
		const math::Vector3& v0 = vertices[vertexIndex[index * 3]];
		const math::Vector3& v1 = vertices[vertexIndex[index * 3 + 1]];
		const math::Vector3& v2 = vertices[vertexIndex[index * 3 + 2]];
		math::Vector3 e0 = (v1 - v0).Normalize();
		math::Vector3 e1 = (v2 - v1).Normalize();
		N = math::Vector3::Cross(e0, e1).Normalize();
		const math::Vector2& st0 = stCoordinates[vertexIndex[index * 3]];
		const math::Vector2& st1 = stCoordinates[vertexIndex[index * 3 + 1]];
		const math::Vector2& st2 = stCoordinates[vertexIndex[index * 3 + 2]];
		st = st0 * (1 - uv.x - uv.y) + st1 * uv.x + st2 * uv.y;
	}

	math::Vector3 evalDiffuseColor(const math::Vector2& st) const override
	{
		float scale = 5;
		float pattern = (math::Fmod(st.x * scale, 1) > 0.5) ^ (math::Fmod(st.y * scale, 1) > 0.5);
		return math::Lerp(math::Vector3(0.815, 0.235, 0.031), math::Vector3(0.937, 0.937, 0.231), pattern);
	}

	std::unique_ptr<math::Vector3[]> vertices;
	uint32_t numTriangles;
	std::unique_ptr<uint32_t[]> vertexIndex;
	std::unique_ptr<math::Vector2[]> stCoordinates;
};