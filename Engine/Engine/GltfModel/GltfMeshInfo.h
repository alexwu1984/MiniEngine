#pragma once
#include "math/vector4.h"
#include "math/vector2.h"
#include "tinygltf/tiny_gltf.h"

namespace Engine
{
	static const int MAX_MATRICES = 200;
	// max bones referenced by a single skinned model
#define max_NUM_MODEL_BONE 100

// max bone influences per vertex
#define max_BONES_PER_VEREX 4
	// bone indices affecting this vertex
	struct VertexBoneID
	{
		VertexBoneID()
		{
			memset(BoneIDs, 0, max_BONES_PER_VEREX * sizeof(uint16_t));
		}
		uint16_t BoneIDs[max_BONES_PER_VEREX];
	};
	// bone blend weights for this vertex
	struct VertexBoneWeight
	{
		VertexBoneWeight()
		{
			memset(BoneWeights, 0, max_BONES_PER_VEREX * sizeof(float));
		}
		float BoneWeights[max_BONES_PER_VEREX];
	};

	/****************************************************
	* CPU mesh payload: positions, normals, UVs, indices, optional skin weights.
	****************************************************/
	struct GltfMeshInfo
	{
		// vertex count
		uint32_t nNumVertices = 0;
		// triangle count (number of faces)
		uint32_t nNumFaces = 0;
		// vertex positions
		math::Vector3* Vertices = nullptr;
		// vertex normals
		math::Vector3* Normals = nullptr;
		// texture coordinates
		math::Vector2* TextureCoords = nullptr;
		// tangent frame (xyz tangent, w handedness)
		math::Vector4* Tangents = nullptr;
		// triangle index buffer (16- or 32-bit)
		uint16_t* FacesIndex = nullptr;
		uint32_t* FacesIndex32 = nullptr;
		int type = 0;
		// optional skinning data
		VertexBoneID* BoneIDs = nullptr;
		VertexBoneWeight* BoneWeights = nullptr;
	};
}