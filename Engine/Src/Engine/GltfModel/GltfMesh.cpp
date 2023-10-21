#include "GltfModel/GltfMesh.h"
#include "GltfModel/GltfMeshBuffer.h"
#include "GltfModel/GltfNode.h"
#include "GltfModel/GltfMaterial.h"
#include "GltfModel/GltfModel.h"
#include "GltfModel/GltfSkeleton.h"
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
		std::vector<std::shared_ptr<uint16_t>> DataBuffer;
		int32_t NodeID = -1;
		int32_t SkinID = -1;

		Matrix4x4 MeshMat;
		GltfModel* Owner;
	};

	GltfMesh::GltfMesh(tinygltf::Model* Model, GltfModel* Owner)
		:GltfModelBase(Model),
		Impl(std::make_shared<GltfMeshP>())
	{
		Impl->Model = Model;
		Impl->Owner = Owner;
		Impl->Mesh = std::make_shared<GltfMeshInfo>();
		Impl->MeshBuffer = std::make_shared<GltfMeshBuffer>();
	}

	GltfMesh::~GltfMesh()
	{

	}

	void GltfMesh::Init(uint32_t MeshIndex, uint32_t PrimitiveIndex, const std::vector < std::shared_ptr<GltfMaterial>>& ModelMatrial, std::shared_ptr< GltfNode> ModelNode)
	{
		auto& meshPrimitive = Impl->Model->meshes[MeshIndex].primitives[PrimitiveIndex];
		Impl->MeshName = Impl->Model->meshes[MeshIndex].name;

		auto Index = Getdata(meshPrimitive.indices, Impl->Mesh->nNumFaces, Impl->Mesh->type);
		Impl->Mesh->nNumFaces /= 3;
		if (Impl->Mesh->type == TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT)
		{
			Impl->Mesh->FacesIndex = (uint16_t*)Index;
		}
		else if (Impl->Mesh->type == TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE)
		{

			std::shared_ptr<uint16_t> TmpData(new uint16_t[Impl->Mesh->nNumFaces * 3], [](uint16_t* p) {delete[]p; });
			uint8_t* pSrc = (uint8_t*)Index;
			for (uint32_t i = 0; i < Impl->Mesh->nNumFaces * 3; ++i)
			{
				TmpData.get()[i] = pSrc[i];
			}
			Impl->Mesh->FacesIndex = TmpData.get();
			Impl->DataBuffer.push_back(TmpData);
		}
		else
		{
			Impl->Mesh->FacesIndex32 = (uint32_t*)Index;
		}

		for (const auto& attribute : meshPrimitive.attributes) {

			int type = 0;
			if (attribute.first == "POSITION")
			{
				Impl->Mesh->Vertices = (Vector3*)Getdata(attribute.second, Impl->Mesh->nNumVertices, type);
				auto& minVaue = Impl->Model->accessors[attribute.second].minValues;
				auto& maxVaue = Impl->Model->accessors[attribute.second].maxValues;
				if (minVaue.size() == 3 && maxVaue.size() == 3)
				{
					Impl->BoundingBox.Set(Vector3(float(maxVaue[0]), float(maxVaue[1]), float(maxVaue[2])), Vector3(float(minVaue[0]), float(minVaue[1]), float(minVaue[2])));
				}

			}
			else if (attribute.first == "NORMAL")
			{
				Impl->Mesh->Normals = (Vector3*)Getdata(attribute.second, Impl->Mesh->nNumVertices, type);
			}
			else if (attribute.first == "TEXCOORD_0")
			{
				Impl->Mesh->TextureCoords = (Vector2*)Getdata(attribute.second, Impl->Mesh->nNumVertices, type);
			}
			else if (attribute.first == "TANGENT")
			{
				Impl->Mesh->Tangents = (Vector4*)Getdata(attribute.second, Impl->Mesh->nNumVertices, type);
			}
			else if (attribute.first == "JOINTS_0")
			{
				Impl->Mesh->BoneIDs = (VertexBoneID*)Getdata(attribute.second, Impl->Mesh->nNumVertices, type);
			}
			else if (attribute.first == "WEIGHTS_0")
			{
				Impl->Mesh->BoneWeights = (VertexBoneWeight*)Getdata(attribute.second, Impl->Mesh->nNumVertices, type);
			}
		}

		int nMaterial = meshPrimitive.material >= 0 ? meshPrimitive.material : 0;
		Impl->Material = ModelMatrial[nMaterial];

		auto& Nodes = Impl->Model->nodes;
		for (int i = 0; i < Nodes.size(); i++)
		{
			if (Nodes[i].mesh == MeshIndex)
			{
				Impl->NodeID = i;
				Impl->SkinID = Nodes[i].skin;

				auto& AllNodeInfos = ModelNode->GetAllNodes();
				if (Impl->NodeID < AllNodeInfos.size())
				{
					Impl->MeshMat = AllNodeInfos[Impl->NodeID]->FinalMeshMat;
				}

				break;
			}

		}

		Impl->MeshBuffer->InitMesh(Impl->Mesh);
	}

	bool GltfMesh::HasSkin() const
	{
		return Impl->Mesh->BoneWeights != nullptr;
	}

	const math::AABB3& GltfMesh::GetBoundingBox() const
	{
		return Impl->BoundingBox;
	}

	const math::Matrix4x4& GltfMesh::GetMeshMat() const
	{
		return Impl->MeshMat;
	}

	std::shared_ptr<GltfMeshBuffer> GltfMesh::GetMeshBuffer()
	{
		return Impl->MeshBuffer;
	}

	std::shared_ptr<Engine::GltfMaterial> GltfMesh::GetMaterial()
	{
		return Impl->Material;
	}

	std::string GltfMesh::GetMeshName() const
	{
		return Impl->MeshName;
	}

	int32_t GltfMesh::GetNodeId() const
	{
		return Impl->NodeID;
	}

	int32_t GltfMesh::GetSkinId() const
	{
		return Impl->SkinID;
	}

	void GltfMesh::SetMeshMat(const math::Matrix4x4& Mat)
	{
		Impl->MeshMat = Mat;
	}

	std::vector<std::vector<Engine::BoneSkinInfo>>& GltfMesh::GetBoneNodeArray()
	{
		assert(Impl->Owner->GetSkeleton());
		return Impl->Owner->GetSkeleton()->GetBoneNodeArray();
	}

}
