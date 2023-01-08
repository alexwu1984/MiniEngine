#include "GltfModel/GltfMesh.h"
#include "GltfModel/GltfMeshBuffer.h"
#include "GltfModel/GltfNode.h"
#include "GltfModel/GltfMaterial.h"
#include "math/matrix4x4.h"

namespace Engine
{
	using namespace math;
	
	struct GltfMeshP
	{
		tinygltf::Model* Model = nullptr;
		std::shared_ptr<GltfMeshInfo> Mesh;
		std::shared_ptr<GltfMeshBuffer> MeshBuffer;
		std::shared_ptr<GltfMaterial> Material;
		AABB3 BoundingBox;
		std::string MeshName;
		std::vector<std::any> DataBuffer;

		int32_t NodeID = -1;
		int32_t SkinID = -1;

		Matrix4x4 MeshMat;
	};

	GltfMesh::GltfMesh(tinygltf::Model* Model)
		:Data(std::make_shared<GltfMeshP>())
	{
		Data->Model = Model;
		Data->Mesh = std::make_shared<GltfMeshInfo>();
		Data->MeshBuffer = std::make_shared<GltfMeshBuffer>();
	}

	GltfMesh::~GltfMesh()
	{

	}

	void GltfMesh::Init(uint32_t MeshIndex, uint32_t PrimitiveIndex, const std::vector < std::shared_ptr<GltfMaterial>>& ModelMatrial, std::shared_ptr< GltfNode> ModelNode)
	{
		auto& meshPrimitive = Data->Model->meshes[MeshIndex].primitives[PrimitiveIndex];
		Data->MeshName = Data->Model->meshes[MeshIndex].name;

		auto Index = Getdata(meshPrimitive.indices, Data->Mesh->nNumFaces, Data->Mesh->type);
		Data->Mesh->nNumFaces /= 3;
		if (Data->Mesh->type == TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT)
		{
			Data->Mesh->FacesIndex = (uint16_t*)Index;
		}
		else if (Data->Mesh->type == TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE)
		{

			std::shared_ptr<uint16_t> TmpData(new uint16_t[Data->Mesh->nNumFaces * 3], [](uint16_t* p) {delete[]p; });
			uint8_t* pSrc = (uint8_t*)Index;
			for (uint32_t i = 0; i < Data->Mesh->nNumFaces * 3; ++i)
			{
				TmpData.get()[i] = pSrc[i];
			}
			Data->Mesh->FacesIndex = TmpData.get();
			Data->DataBuffer.push_back(Data);
		}
		else
		{
			Data->Mesh->FacesIndex32 = (uint32_t*)Index;
		}

		for (const auto& attribute : meshPrimitive.attributes) {

			int type = 0;
			if (attribute.first == "POSITION")
			{
				Data->Mesh->Vertices = (Vector3*)Getdata(attribute.second, Data->Mesh->nNumVertices, type);
				auto& minVaue = Data->Model->accessors[attribute.second].minValues;
				auto& maxVaue = Data->Model->accessors[attribute.second].maxValues;
				if (minVaue.size() == 3 && maxVaue.size() == 3)
				{
					Data->BoundingBox.Set(Vector3(float(maxVaue[0]), float(maxVaue[1]), float(maxVaue[2])), Vector3(float(minVaue[0]), float(minVaue[1]), float(minVaue[2])));
				}

			}
			else if (attribute.first == "NORMAL")
			{
				Data->Mesh->Normals = (Vector3*)Getdata(attribute.second, Data->Mesh->nNumVertices, type);
			}
			else if (attribute.first == "TEXCOORD_0")
			{
				Data->Mesh->TextureCoords = (Vector2*)Getdata(attribute.second, Data->Mesh->nNumVertices, type);
			}
			else if (attribute.first == "TANGENT")
			{
				Data->Mesh->Tangents = (Vector4*)Getdata(attribute.second, Data->Mesh->nNumVertices, type);
			}
			else if (attribute.first == "JOINTS_0")
			{
				Data->Mesh->BoneIDs = (VertexBoneID*)Getdata(attribute.second, Data->Mesh->nNumVertices, type);
			}
			else if (attribute.first == "WEIGHTS_0")
			{
				Data->Mesh->BoneWeights = (VertexBoneWeight*)Getdata(attribute.second, Data->Mesh->nNumVertices, type);
			}
		}

		int nMaterial = meshPrimitive.material >= 0 ? meshPrimitive.material : 0;
		Data->Material = ModelMatrial[nMaterial];

		auto& Nodes = Data->Model->nodes;
		for (int i = 0; i < Nodes.size(); i++)
		{
			if (Nodes[i].mesh == MeshIndex)
			{
				Data->NodeID = i;
				Data->SkinID = Nodes[i].skin;

				auto& AllNodeInfos = ModelNode->GetAllNodes();
				if (Data->NodeID < AllNodeInfos.size())
				{
					Data->MeshMat = AllNodeInfos[Data->NodeID]->FinalMeshMat;
				}

				break;
			}

		}

		Data->MeshBuffer->InitMesh(Data->Mesh);
	}

	bool GltfMesh::HasSkin() const
	{
		return Data->Mesh->BoneWeights != nullptr;
	}

	const math::AABB3& GltfMesh::GetBoundingBox() const
	{
		return Data->BoundingBox;
	}

	const math::Matrix4x4& GltfMesh::GetMeshMat() const
	{
		return Data->MeshMat;
	}

	void* GltfMesh::Getdata(int32_t attributeIndex, uint32_t& nCount, int32_t& CommpontType)
	{
		const auto& indicesAccessor = Data->Model->accessors[attributeIndex];
		const auto& bufferView = Data->Model->bufferViews[indicesAccessor.bufferView];
		const auto& buffer = Data->Model->buffers[bufferView.buffer];
		const auto dataAddress = buffer.data.data() + bufferView.byteOffset +
			indicesAccessor.byteOffset;
		const auto byteStride = indicesAccessor.ByteStride(bufferView);
		nCount = uint32_t(indicesAccessor.count);
		CommpontType = indicesAccessor.componentType;


		int type = indicesAccessor.type;
		int nStep = 0;
		if (type == TINYGLTF_TYPE_SCALAR) {
			nStep = 1;
		}
		else if (type == TINYGLTF_TYPE_VEC2) {
			nStep = 2;
		}
		else if (type == TINYGLTF_TYPE_VEC3) {

			nStep = 3;
		}
		else if (type == TINYGLTF_TYPE_VEC4) {

			nStep = 4;
		}
		int OneSize = 0;

		if (CommpontType == 5122 || CommpontType == 5123) {
			OneSize = sizeof(uint16_t);
		}
		else if (CommpontType == 5124 || CommpontType == 5125) {
			OneSize = sizeof(uint32_t);
		}
		else if (CommpontType == 5126) {

			OneSize = sizeof(float);
		}
		else if (CommpontType == 5120 || CommpontType == 5121) {
			OneSize = sizeof(uint8_t);
		}

		if (nStep == 0 || OneSize == 0 || nStep * OneSize == byteStride)
		{
			return (void*)dataAddress;
		}
		else
		{
			std::shared_ptr<uint8_t> TmpData(new uint8_t[nStep * OneSize * nCount], [](uint8_t* p) {delete[]p; });

			uint8_t* pSrc = (uint8_t*)dataAddress;
			for (uint32_t i = 0; i < nCount; ++i)
			{

				memcpy(TmpData.get() + i * OneSize * nStep, pSrc + i * byteStride, OneSize * nStep);
			}
			Data->DataBuffer.push_back(TmpData);

			return (void*)TmpData.get();
		}
	}

}
