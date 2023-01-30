#pragma once
#include "GltfModel/GltfMeshInfo.h"
#include "RHI/RHIDefinitions.h"
#include "RHI/RHIVertexBuffer.h"
#include "RHI/RHIIndexBuffer.h"

namespace Engine
{
	struct GltfMeshBufferP;

	class GltfMeshBuffer
	{

	public:
		GltfMeshBuffer();
		~GltfMeshBuffer();

		void InitMesh(std::shared_ptr< GltfMeshInfo> MeshInfo);
		void UpdateVert(math::Vector3* pVert, int nVert);

		std::array<std::shared_ptr<RenderCore::RHIVertexBuffer>, RenderCore::EVertexType::VT_Max>& GetVerticesBuffer();
		std::shared_ptr<RenderCore::RHIIndexBuffer> GetIndexBuffer() const;

	private:
		std::shared_ptr<GltfMeshBufferP> Impl;
	};
}