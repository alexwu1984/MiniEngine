#pragma once
#include "GltfModel/GltfMeshInfo.h"
#include "RHI/RHIVertexBuffer.h"
#include "RHI/RHIIndexBuffer.h"

namespace Engine
{
	struct GltfMeshBufferP;

	class GltfMeshBuffer
	{
	public:
		enum VertexType : uint8_t
		{
			VT_Position = 0,
			VT_Normal = 1,
			VT_UV0 = 2,
			VT_Tangent = 3,
			VT_JointsWeights0 = 4,
			VT_JointsIndices0 = 5,
			VT_Max
		};
	public:
		GltfMeshBuffer();
		~GltfMeshBuffer();

		void InitMesh(std::shared_ptr< GltfMeshInfo> MeshInfo);
		void UpdateVert(math::Vector3* pVert, int nVert);

		std::array<std::shared_ptr<RenderCore::RHIVertexBuffer>, VT_Max>& GetVerticesBuffer();

	private:
		std::shared_ptr<GltfMeshBufferP> Impl;
	};
}