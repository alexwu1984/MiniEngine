#include "GltfModel/GltfMesh.h"
#include "GltfModel/GltfMeshBuffer.h"
#include "GltfModel/GltfNode.h"
#include "Material/GltfMaterial.h"
#include "GltfModel/GltfModel.h"
#include "GltfModel/GltfSkeleton.h"
#include "math/matrix4x4.h"

namespace Engine
{
	using namespace math;
	
	struct GltfMeshPrivate
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
		d_ptr(new GltfMeshPrivate())
	{
		C_P(GltfMesh);
		d->Model = Model;
		d->Owner = Owner;
		d->Mesh = std::make_shared<GltfMeshInfo>();
		d->MeshBuffer = std::make_shared<GltfMeshBuffer>();
	}

	GltfMesh::~GltfMesh()
	{
		delete d_ptr;
	}

	void GltfMesh::Init(uint32_t MeshIndex, uint32_t PrimitiveIndex, const std::vector < std::shared_ptr<GltfMaterial>>& ModelMatrial, std::shared_ptr< GltfNode> ModelNode)
	{
		C_P(GltfMesh);
		auto& meshPrimitive = d->Model->meshes[MeshIndex].primitives[PrimitiveIndex];
		d->MeshName = d->Model->meshes[MeshIndex].name;

		auto Index = Getdata(meshPrimitive.indices, d->Mesh->nNumFaces, d->Mesh->type);
		d->Mesh->nNumFaces /= 3;
		if (d->Mesh->type == TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT)
		{
			d->Mesh->FacesIndex = (uint16_t*)Index;
		}
		else if (d->Mesh->type == TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE)
		{

			std::shared_ptr<uint16_t> TmpData(new uint16_t[d->Mesh->nNumFaces * 3], [](uint16_t* p) {delete[]p; });
			uint8_t* pSrc = (uint8_t*)Index;
			for (uint32_t i = 0; i < d->Mesh->nNumFaces * 3; ++i)
			{
				TmpData.get()[i] = pSrc[i];
			}
			d->Mesh->FacesIndex = TmpData.get();
			d->DataBuffer.push_back(TmpData);
		}
		else
		{
			d->Mesh->FacesIndex32 = (uint32_t*)Index;
		}

		for (const auto& attribute : meshPrimitive.attributes) {

			int type = 0;
			if (attribute.first == "POSITION")
			{
				d->Mesh->Vertices = (Vector3*)Getdata(attribute.second, d->Mesh->nNumVertices, type);
				auto& minVaue = d->Model->accessors[attribute.second].minValues;
				auto& maxVaue = d->Model->accessors[attribute.second].maxValues;
				if (minVaue.size() == 3 && maxVaue.size() == 3)
				{
					d->BoundingBox.Set(Vector3(float(maxVaue[0]), float(maxVaue[1]), float(maxVaue[2])), Vector3(float(minVaue[0]), float(minVaue[1]), float(minVaue[2])));
				}

			}
			else if (attribute.first == "NORMAL")
			{
				d->Mesh->Normals = (Vector3*)Getdata(attribute.second, d->Mesh->nNumVertices, type);
			}
			else if (attribute.first == "TEXCOORD_0")
			{
				d->Mesh->TextureCoords = (Vector2*)Getdata(attribute.second, d->Mesh->nNumVertices, type);
			}
			else if (attribute.first == "TANGENT")
			{
				d->Mesh->Tangents = (Vector4*)Getdata(attribute.second, d->Mesh->nNumVertices, type);
			}
			else if (attribute.first == "JOINTS_0")
			{
				d->Mesh->BoneIDs = (VertexBoneID*)Getdata(attribute.second, d->Mesh->nNumVertices, type);
			}
			else if (attribute.first == "WEIGHTS_0")
			{
				d->Mesh->BoneWeights = (VertexBoneWeight*)Getdata(attribute.second, d->Mesh->nNumVertices, type);
			}
		}

		int nMaterial = meshPrimitive.material >= 0 ? meshPrimitive.material : 0;
		d->Material = ModelMatrial[nMaterial];

		auto& Nodes = d->Model->nodes;
		for (int i = 0; i < Nodes.size(); i++)
		{
			if (Nodes[i].mesh == MeshIndex)
			{
				d->NodeID = i;
				d->SkinID = Nodes[i].skin;

				auto& AllNodeInfos = ModelNode->GetAllNodes();
				if (d->NodeID < AllNodeInfos.size())
				{
					d->MeshMat = AllNodeInfos[d->NodeID]->FinalMeshMat;
				}

				break;
			}

		}

		d->MeshBuffer->InitMesh(d->Mesh);
	}

	bool GltfMesh::HasSkin() const
	{
		C_P(GltfMesh);
		return d->Mesh->BoneWeights != nullptr;
	}

	const math::AABB3& GltfMesh::GetBoundingBox() const
	{
		C_P(GltfMesh);
		return d->BoundingBox;
	}

	const math::Matrix4x4& GltfMesh::GetMeshMat() const
	{
		C_P(GltfMesh);
		return d->MeshMat;
	}

	std::shared_ptr<GltfMeshBuffer> GltfMesh::GetMeshBuffer()
	{
		C_P(GltfMesh);
		return d->MeshBuffer;
	}

	std::shared_ptr<MaterialBase> GltfMesh::GetMaterial()
	{
		C_P(GltfMesh);
		return d->Material;
	}

	std::string GltfMesh::GetMeshName() const
	{
		C_P(GltfMesh);
		return d->MeshName;
	}

	int32_t GltfMesh::GetNodeId() const
	{
		C_P(GltfMesh);
		return d->NodeID;
	}

	int32_t GltfMesh::GetSkinId() const
	{
		C_P(GltfMesh);
		return d->SkinID;
	}

	void GltfMesh::SetMeshMat(const math::Matrix4x4& Mat)
	{
		C_P(GltfMesh);
		d->MeshMat = Mat;
	}

	std::vector<std::vector<Engine::BoneSkinInfo>>& GltfMesh::GetBoneNodeArray()
	{
		C_P(GltfMesh);
		assert(d->Owner->GetSkeleton());
		return d->Owner->GetSkeleton()->GetBoneNodeArray();
	}

}
