#pragma once
#include "GltfModel/GltfMeshInfo.h"
#include "RHI/RHIDefinitions.h"
#include "RHI/RHIVertexBuffer.h"
#include "RHI/RHIIndexBuffer.h"

namespace Engine
{
	struct GltfMeshBufferPrivate;

	/** Set synchronously in InitMesh from GltfMeshInfo — do not infer layout from RHIVertexBuffer pointers (those fill asynchronously). */
	struct MeshBufferVertexFeatures
	{
		static constexpr uint32_t Tangent = 1u << 0;
		static constexpr uint32_t Skinning = 1u << 1;
	};

	class GltfMeshBuffer
	{

	public:
		GltfMeshBuffer();
		~GltfMeshBuffer();

		void InitMesh(std::shared_ptr< GltfMeshInfo> MeshInfo);
		void UpdateVert(math::Vector3* pVert, int nVert);
		void UpdateNormal(math::Vector3* Normal, int nVert);
		void UpdateTangent(math::Vector4* Tangent, int nVert);

		std::array<std::shared_ptr<RenderCore::RHIVertexBuffer>, RenderCore::EVertexType::VT_Max>& GetVerticesBuffer();
		std::shared_ptr<RenderCore::RHIIndexBuffer> GetIndexBuffer() const;

		uint32_t GetDeclaredVertexFeatures() const noexcept;

	private:
		GltfMeshBufferPrivate* d_ptr = nullptr;
	};
}