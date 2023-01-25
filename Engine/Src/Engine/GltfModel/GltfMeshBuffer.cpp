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

		int AtrributeCount = 3;
	};

	GltfMeshBuffer::GltfMeshBuffer()
		:Impl(std::make_shared<GltfMeshBufferP>())
	{

	}

	GltfMeshBuffer::~GltfMeshBuffer()
	{

	}

	void GltfMeshBuffer::InitMesh(std::shared_ptr< GltfMeshInfo> MeshInfo)
	{
		auto CreateVertexBufferCommand = [MeshInfo = MeshInfo, Impl = Impl]() {
			auto RHI = GEngine->GetRHI();
			Impl->VerticesBuffer[VT_Position] = RHI->RHICreateVertexBuffer(MeshInfo->Vertices, RenderCore::BUF_Dynamic, sizeof(math::Vector3), MeshInfo->nNumVertices);
			Impl->VerticesBuffer[VT_Normal] = RHI->RHICreateVertexBuffer(MeshInfo->Normals, RenderCore::BUF_Dynamic, sizeof(math::Vector3), MeshInfo->nNumVertices);
			if (MeshInfo->TextureCoords)
			{
				Impl->VerticesBuffer[VT_UV0] = RHI->RHICreateVertexBuffer(MeshInfo->TextureCoords, RenderCore::BUF_Dynamic, sizeof(math::Vector2), MeshInfo->nNumVertices);

			}
			else
			{
				//If TextureCoords is null,use normal instead of TextureCoords
				Impl->VerticesBuffer[VT_UV0] = RHI->RHICreateVertexBuffer(MeshInfo->Normals, RenderCore::BUF_Dynamic, sizeof(math::Vector2), MeshInfo->nNumVertices);
			}
			if (MeshInfo->Tangents)
			{
				Impl->VerticesBuffer[VT_Tangent] = RHI->RHICreateVertexBuffer(MeshInfo->Tangents, RenderCore::BUF_Dynamic, sizeof(math::Vector4), MeshInfo->nNumVertices);
				Impl->AtrributeCount += 1;
			}

			// If BoneID not exist,It's no necessary to create JointsWeights0
			if (MeshInfo->BoneIDs)
			{
				Impl->VerticesBuffer[VT_JointsWeights0] = RHI->RHICreateVertexBuffer(MeshInfo->BoneWeights->BoneWeights, RenderCore::BUF_Dynamic, sizeof(math::Vector4), MeshInfo->nNumVertices);
				int nSize = MeshInfo->nNumVertices * 4;
				std::vector<float> BondId;
				BondId.resize(nSize);
				uint16_t* pSrcID = (uint16_t*)MeshInfo->BoneIDs->BoneIDs;
				for (int i = 0; i < nSize; i++)
				{
					BondId[i] = (float)pSrcID[i];
				}

				Impl->VerticesBuffer[VT_JointsIndices0] = RHI->RHICreateVertexBuffer(BondId.data(), RenderCore::BUF_Dynamic, sizeof(math::Vector4), MeshInfo->nNumVertices);

				Impl->AtrributeCount += 2;
			}

			if (MeshInfo->FacesIndex)
			{
				Impl->IndexBuffer = RHI->RHICreateIndexBuffer(MeshInfo->FacesIndex, RenderCore::BUF_IndexBuffer, MeshInfo->nNumFaces);
			}
			else
			{
				Impl->IndexBuffer = RHI->RHICreateIndexBuffer(MeshInfo->FacesIndex32, RenderCore::BUF_IndexBuffer, MeshInfo->nNumFaces);
			}

		};

		ENQUEUE_UNIQUE_RENDER_COMMAND(CreateVertexBufferCommand);
	}

	void GltfMeshBuffer::UpdateVert(math::Vector3* pVert, int nVert)
	{
		ENQUEUE_UNIQUE_RENDER_COMMAND(([pVert, nVert, Impl = Impl](){
			auto RHI = GEngine->GetRHI();
			if (Impl->VerticesBuffer[VT_Position])
			{
				RHI->RHIUpdateVertexBuffer(Impl->VerticesBuffer[VT_Position], pVert, nVert, sizeof(math::Vector3));
			}
		}));
	}

	std::array<std::shared_ptr<RenderCore::RHIVertexBuffer>, Engine::GltfMeshBuffer::VT_Max>& GltfMeshBuffer::GetVerticesBuffer()
	{
		return Impl->VerticesBuffer;
	}

}