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
		uint32_t DeclaredVertexFeatures = 0;
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
		{
			C_P(GltfMeshBuffer);
			uint32_t Feat = 0;
			if (MeshInfo && MeshInfo->Tangents)
				Feat |= MeshBufferVertexFeatures::Tangent;
			if (MeshInfo && MeshInfo->BoneIDs && MeshInfo->BoneWeights)
				Feat |= MeshBufferVertexFeatures::Skinning;
			d->DeclaredVertexFeatures = Feat;
		}

		// If UVs are missing, feed a zero UV stream (do NOT alias normals with a smaller stride).
		std::shared_ptr<std::vector<math::Vector2>> FallbackUVs;
		if (!MeshInfo->TextureCoords && MeshInfo->nNumVertices > 0)
		{
			FallbackUVs = std::make_shared<std::vector<math::Vector2>>(MeshInfo->nNumVertices, math::Vector2(0.0f, 0.0f));
		}

		auto CreateVertexBufferCommand = [MeshInfo = MeshInfo, FallbackUVs, this](RenderCore::DynamicRHI* RHI) {
			C_P(GltfMeshBuffer);
			d->VerticesBuffer[RenderCore::EVertexType::VT_Position] = RHI->RHICreateVertexBuffer(MeshInfo->Vertices, RenderCore::BUF_Dynamic, sizeof(math::Vector3), MeshInfo->nNumVertices);
			d->VerticesBuffer[RenderCore::EVertexType::VT_Normal] = RHI->RHICreateVertexBuffer(MeshInfo->Normals, RenderCore::BUF_Dynamic, sizeof(math::Vector3), MeshInfo->nNumVertices);
			if (MeshInfo->TextureCoords)
			{
				d->VerticesBuffer[RenderCore::EVertexType::VT_UV0] = RHI->RHICreateVertexBuffer(MeshInfo->TextureCoords, RenderCore::BUF_Dynamic, sizeof(math::Vector2), MeshInfo->nNumVertices);

			}
			else if (FallbackUVs)
			{
				d->VerticesBuffer[RenderCore::EVertexType::VT_UV0] = RHI->RHICreateVertexBuffer(FallbackUVs->data(), RenderCore::BUF_Dynamic, sizeof(math::Vector2), MeshInfo->nNumVertices);
			}
			else
			{
				// No UVs and no vertices: create nothing.
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
		auto Vertices = std::make_shared<std::vector<math::Vector3>>(pVert, pVert + nVert);
		auto UpdateVertFun = [Vertices, nVert, this](RenderCore::DynamicRHI * RHI) {
			C_P(GltfMeshBuffer);
			if (d->VerticesBuffer[RenderCore::EVertexType::VT_Position])
			{
				RHI->RHIUpdateVertexBuffer(d->VerticesBuffer[RenderCore::EVertexType::VT_Position], Vertices->data(), nVert, sizeof(math::Vector3));
			}
		};
		ENQUEUE_UNIQUE_RENDER_COMMAND(UpdateVertFun);
	}

	void GltfMeshBuffer::UpdateNormal(math::Vector3* Normal, int nVert)
	{
		auto Normals = std::make_shared<std::vector<math::Vector3>>(Normal, Normal + nVert);
		auto UpdateNormalFun = [Normals, nVert, this](RenderCore::DynamicRHI* RHI) {
			C_P(GltfMeshBuffer);
			if (d->VerticesBuffer[RenderCore::EVertexType::VT_Normal])
			{
				RHI->RHIUpdateVertexBuffer(d->VerticesBuffer[RenderCore::EVertexType::VT_Normal], Normals->data(), nVert, sizeof(math::Vector3));
			}
		};
		ENQUEUE_UNIQUE_RENDER_COMMAND(UpdateNormalFun);
	}

	void GltfMeshBuffer::UpdateTangent(math::Vector4* Tangent, int nVert)
	{
		auto Tangents = std::make_shared<std::vector<math::Vector4>>(Tangent, Tangent + nVert);
		auto UpdateTangentFun = [Tangents, nVert, this](RenderCore::DynamicRHI* RHI) {
			C_P(GltfMeshBuffer);
			if (d->VerticesBuffer[RenderCore::EVertexType::VT_Tangent])
			{
				RHI->RHIUpdateVertexBuffer(d->VerticesBuffer[RenderCore::EVertexType::VT_Tangent], Tangents->data(), nVert, sizeof(math::Vector4));
			}
		};
		ENQUEUE_UNIQUE_RENDER_COMMAND(UpdateTangentFun);
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

	uint32_t GltfMeshBuffer::GetDeclaredVertexFeatures() const noexcept
	{
		C_P(const GltfMeshBuffer);
		return d->DeclaredVertexFeatures;
	}

}