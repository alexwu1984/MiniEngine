#include "GltfModel/GltfMeshBuffer.h"
#include "Thread/RenderThread.h"
#include "RHI/DynamicRHI.h"
#include "Engine/Engine.h"
#include "RHI/RHIDefinitions.h"

namespace Engine
{
	struct GltfMeshBufferP
	{
		std::array<std::shared_ptr<RenderCore::RHIVertexBuffer>, GltfMeshBuffer::VT_Max> VerticesBuffer;
		std::shared_ptr<RenderCore::RHIIndexBuffer> IndexBuffer;

		int AtrributeCount = 4;
	};

	GltfMeshBuffer::GltfMeshBuffer()
		:Data(std::shared_ptr<GltfMeshBufferP>())
	{

	}

	GltfMeshBuffer::~GltfMeshBuffer()
	{

	}

	void GltfMeshBuffer::InitMesh(std::shared_ptr< GltfMeshInfo> MeshInfo)
	{
		ENQUEUE_UNIQUE_RENDER_COMMAND(([MeshInfo = MeshInfo, Data = Data]() {
			auto RHI = GEngine->GetRHI();
			Data->VerticesBuffer[VT_Position] = RHI->RHICreateVertexBuffer(MeshInfo->pVertices, RenderCore::BUF_Dynamic, sizeof(math::Vector3), MeshInfo->nNumVertices);
			Data->VerticesBuffer[VT_Normal] = RHI->RHICreateVertexBuffer(MeshInfo->pNormals, RenderCore::BUF_Dynamic, sizeof(math::Vector3), MeshInfo->nNumVertices);
			if (MeshInfo->pTextureCoords)
			{
				Data->VerticesBuffer[VT_Texcoord] = RHI->RHICreateVertexBuffer(MeshInfo->pTextureCoords, RenderCore::BUF_Dynamic, sizeof(math::Vector2), MeshInfo->nNumVertices);

			}
			else
			{
				Data->VerticesBuffer[VT_Texcoord] = RHI->RHICreateVertexBuffer(MeshInfo->pNormals, RenderCore::BUF_Dynamic, sizeof(math::Vector2), MeshInfo->nNumVertices);
			}
			if (MeshInfo->pTangents)
			{
				Data->VerticesBuffer[VT_Tangent] = RHI->RHICreateVertexBuffer(MeshInfo->pTangents, RenderCore::BUF_Dynamic, sizeof(math::Vector4), MeshInfo->nNumVertices);
			}
			else
			{
				Data->VerticesBuffer[VT_Tangent] = RHI->RHICreateVertexBuffer(MeshInfo->pVertices, RenderCore::BUF_Dynamic, sizeof(math::Vector4), MeshInfo->nNumVertices);
			}
			if (MeshInfo->pBoneIDs)
			{
				int nSize = MeshInfo->nNumVertices * 4;
				std::vector<float> BondId;
				BondId.resize(nSize);
				uint16_t* pSrcID = (uint16_t*)MeshInfo->pBoneIDs->BoneIDs;
				for (int i = 0; i < nSize; i++)
				{
					BondId[i] = (float)pSrcID[i];
				}
				Data->VerticesBuffer[VT_BoneID] = RHI->RHICreateVertexBuffer(BondId.data(), RenderCore::BUF_Dynamic, sizeof(math::Vector4), MeshInfo->nNumVertices);

				Data->VerticesBuffer[VT_BoneWidget] = RHI->RHICreateVertexBuffer(MeshInfo->pBoneWeights->BoneWeights, RenderCore::BUF_Dynamic, sizeof(math::Vector4), MeshInfo->nNumVertices);
				Data->AtrributeCount += 2;
			}

			if (MeshInfo->pFacesIndex)
			{
				Data->IndexBuffer = RHI->RHICreateIndexBuffer(MeshInfo->pFacesIndex, RenderCore::BUF_IndexBuffer, MeshInfo->nNumFaces);
			}
			else
			{
				Data->IndexBuffer = RHI->RHICreateIndexBuffer(MeshInfo->pFacesIndex32, RenderCore::BUF_IndexBuffer, MeshInfo->nNumFaces);
			}

		}));
	}

	void GltfMeshBuffer::UpdateVert(math::Vector3* pVert, int nVert)
	{
		ENQUEUE_UNIQUE_RENDER_COMMAND(([pVert, nVert, Data = Data](){
			auto RHI = GEngine->GetRHI();
			if (Data->VerticesBuffer[VT_Position])
			{
				RHI->RHIUpdateVertexBuffer(Data->VerticesBuffer[VT_Position], pVert, nVert, sizeof(math::Vector3));
			}
		}));
	}

	std::array<std::shared_ptr<RenderCore::RHIVertexBuffer>, Engine::GltfMeshBuffer::VT_Max>& GltfMeshBuffer::GetVerticesBuffer()
	{
		return Data->VerticesBuffer;
	}

}