#pragma once
#include "math/vector4.h"
#include "math/vector2.h"
#include "tinygltf/tiny_gltf.h"

namespace Engine
{
	static const int MAX_MATRICES = 200;
	//一个3DModel拥有的最大骨骼数
#define max_NUM_MODEL_BONE 100

//每一个顶点最多受到*个骨骼影响
#define max_BONES_PER_VEREX 4
	//每个顶点被骨骼IDs所影响
	struct VertexBoneID
	{
		VertexBoneID()
		{
			memset(BoneIDs, 0, max_BONES_PER_VEREX * sizeof(uint16_t));
		}
		uint16_t BoneIDs[max_BONES_PER_VEREX];
	};
	//每个顶点骨骼影响的权重
	struct VertexBoneWeight
	{
		VertexBoneWeight()
		{
			memset(BoneWeights, 0, max_BONES_PER_VEREX * sizeof(float));
		}
		float BoneWeights[max_BONES_PER_VEREX];
	};

	/****************************************************
*	Mesh结构体
*	每个Mesh包含：
*		1、顶点数目
*		2、顶点位置
*		3、顶点颜色
*		4、顶点法向量
*		5、顶点纹理采样坐标
*		6、Faces数量
*		7、Faces索引
*		8、BoneIDs 影响该顶点的骨骼ID
*		9、BoneWeights 影响该顶点的骨骼权重
*		10、mesh使用的材质索引
*		11、mesh相对model的变化矩阵
****************************************************/
	struct GltfMeshInfo
	{
		GltfMeshInfo()
		{
			nNumVertices = 0;
			nNumFaces = 0;
			pVertices = nullptr;
			pNormals = nullptr;
			pTextureCoords = nullptr;
			pFacesIndex = nullptr;
			pBoneIDs = nullptr;
			pBoneWeights = nullptr;
			pFacesIndex32 = nullptr;
		}
		//顶点数目
		uint32_t nNumVertices;
		//面数目（三角形数目）
		uint32_t nNumFaces;
		//顶点位置
		math::Vector3* pVertices = nullptr;
		//顶点法向量
		math::Vector3* pNormals = nullptr;
		//顶点纹理采样坐标
		math::Vector2* pTextureCoords = nullptr;
		//切线
		math::Vector4* pTangents = nullptr;
		//faces（三角形）索引
		uint16_t* pFacesIndex = nullptr;
		uint32_t* pFacesIndex32 = nullptr;
		int type;
		////骨骼ID
		VertexBoneID* pBoneIDs;
		////骨骼权重
		VertexBoneWeight* pBoneWeights;
	};
}