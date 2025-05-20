#include "GltfModel/GltfMeshBuffer.h"
#include "Thread/RenderThread.h"
#include "RHI/DynamicRHI.h"
#include "Engine/Engine.h"
#include "RHI/RHIDefinitions.h"

namespace Engine
{
	struct GltfMeshBufferPrivate
	{
		std::array<std::shared_ptr<RenderCore::RHIVertexBuffer>, RenderCore::EVertexType::VT_Max> VerticesBuffer;
		std::shared_ptr<RenderCore::RHIIndexBuffer> IndexBuffer;

		int AtrributeCount = 3;
	};

	GltfMeshBuffer::GltfMeshBuffer()
		:d_ptr(new GltfMeshBufferPrivate())
	{

	}

	GltfMeshBuffer::~GltfMeshBuffer()
	{
		delete d_ptr;
	}

	void GltfMeshBuffer::InitMesh(std::shared_ptr< GltfMeshInfo> MeshInfo)
	{
		auto CreateVertexBufferCommand = [MeshInfo = MeshInfo, this](RenderCore::DynamicRHI* RHI) {
			C_P(GltfMeshBuffer);
			d->VerticesBuffer[RenderCore::EVertexType::VT_Position] = RHI->RHICreateVertexBuffer(MeshInfo->Vertices, RenderCore::BUF_Dynamic, sizeof(math::Vector3), MeshInfo->nNumVertices);
			d->VerticesBuffer[RenderCore::EVertexType::VT_Normal] = RHI->RHICreateVertexBuffer(MeshInfo->Normals, RenderCore::BUF_Dynamic, sizeof(math::Vector3), MeshInfo->nNumVertices);
			if (MeshInfo->TextureCoords)
			{
				d->VerticesBuffer[RenderCore::EVertexType::VT_UV0] = RHI->RHICreateVertexBuffer(MeshInfo->TextureCoords, RenderCore::BUF_Dynamic, sizeof(math::Vector2), MeshInfo->nNumVertices);

			}
			else
			{
				//If TextureCoords is null,use normal instead of TextureCoords
				d->VerticesBuffer[RenderCore::EVertexType::VT_UV0] = RHI->RHICreateVertexBuffer(MeshInfo->Normals, RenderCore::BUF_Dynamic, sizeof(math::Vector2), MeshInfo->nNumVertices);
			}
			if (MeshInfo->Tangents)
			{
				d->VerticesBuffer[RenderCore::EVertexType::VT_Tangent] = RHI->RHICreateVertexBuffer(MeshInfo->Tangents, RenderCore::BUF_Dynamic, sizeof(math::Vector4), MeshInfo->nNumVertices);
				d->AtrributeCount += 1;
			}

			// If BoneID not exist,It's no necessary to create JointsWeights0
			if (MeshInfo->BoneIDs)
			{
				d->VerticesBuffer[RenderCore::EVertexType::VT_JointsWeights0] = RHI->RHICreateVertexBuffer(MeshInfo->BoneWeights->BoneWeights, RenderCore::BUF_Dynamic, sizeof(math::Vector4), MeshInfo->nNumVertices);
				int nSize = MeshInfo->nNumVertices * 4;
				std::vector<float> BondId;
				BondId.resize(nSize);
				uint16_t* pSrcID = (uint16_t*)MeshInfo->BoneIDs->BoneIDs;
				for (int i = 0; i < nSize; i++)
				{
					BondId[i] = (float)pSrcID[i];
				}

				d->VerticesBuffer[RenderCore::EVertexType::VT_JointsIndices0] = RHI->RHICreateVertexBuffer(BondId.data(), RenderCore::BUF_Dynamic, sizeof(math::Vector4), MeshInfo->nNumVertices);
				d->AtrributeCount += 2;
			}

			if (MeshInfo->FacesIndex)
			{
				d->IndexBuffer = RHI->RHICreateIndexBuffer(MeshInfo->FacesIndex, RenderCore::BUF_IndexBuffer, MeshInfo->nNumFaces);
			}
			else
			{
				d->IndexBuffer = RHI->RHICreateIndexBuffer(MeshInfo->FacesIndex32, RenderCore::BUF_IndexBuffer, MeshInfo->nNumFaces);
			}

		};
		ENQUEUE_UNIQUE_RENDER_COMMAND(CreateVertexBufferCommand);
	}

	void GltfMeshBuffer::UpdateVert(math::Vector3* pVert, int nVert)
	{
		auto UpdateVertFun = [pVert, nVert, this](RenderCore::DynamicRHI * RHI) {
			C_P(GltfMeshBuffer);
			if (d->VerticesBuffer[RenderCore::EVertexType::VT_Position])
			{
				RHI->RHIUpdateVertexBuffer(d->VerticesBuffer[RenderCore::EVertexType::VT_Position], pVert, nVert, sizeof(math::Vector3));
			}
		};
		ENQUEUE_UNIQUE_RENDER_COMMAND(UpdateVertFun);
	}

	std::array<std::shared_ptr<RenderCore::RHIVertexBuffer>, RenderCore::EVertexType::VT_Max>& GltfMeshBuffer::GetVerticesBuffer()
	{
		C_P(GltfMeshBuffer);
		return d->VerticesBuffer;
	}

	std::shared_ptr<RenderCore::RHIIndexBuffer> GltfMeshBuffer::GetIndexBuffer() const
	{
		C_P(GltfMeshBuffer);
		return d->IndexBuffer;
	}

}