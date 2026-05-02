#include "GltfModel/GltfMesh.h"
#include "GltfModel/GltfMeshBuffer.h"
#include "GltfModel/GltfNode.h"
#include "Material/GltfMaterial.h"
#include "GltfModel/GltfModel.h"
#include "GltfModel/GltfSkeleton.h"
#include "math/matrix4x4.h"
#include "tinygltf/tiny_gltf.h"
#include <algorithm>
#include <vector>

namespace Engine
{
	using namespace math;

	namespace
	{
		void DfsCollectNodesWithMesh(const tinygltf::Model& model, int nodeIdx, int meshIndex, std::vector<int>& outOrdered)
		{
			if (nodeIdx < 0 || nodeIdx >= static_cast<int>(model.nodes.size()))
				return;
			const tinygltf::Node& n = model.nodes[nodeIdx];
			if (n.mesh == meshIndex)
				outOrdered.push_back(nodeIdx);
			for (int c : n.children)
				DfsCollectNodesWithMesh(model, c, meshIndex, outOrdered);
		}

		/** Node indices that reference meshIndex, in depth-first scene order (matches typical primitive order better than node-array index). */
		void CollectMeshNodesSceneOrder(const tinygltf::Model& model, int meshIndex, std::vector<int>& outOrdered)
		{
			outOrdered.clear();
			int sceneIdx = model.defaultScene;
			if (sceneIdx < 0 || sceneIdx >= static_cast<int>(model.scenes.size()))
				sceneIdx = 0;
			if (sceneIdx < 0 || sceneIdx >= static_cast<int>(model.scenes.size()))
				return;
			const tinygltf::Scene& sc = model.scenes[sceneIdx];
			for (int root : sc.nodes)
				DfsCollectNodesWithMesh(model, root, meshIndex, outOrdered);
		}
	}
	
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

		std::vector<Vector3*> BlendShapes;
		std::vector<Vector3*> BlendShapeNormals;
		std::vector<Vector3*> BlendShapeTangents;
		std::vector<std::string> BlendShapeNames;
		std::shared_ptr<Vector3> BlendVerts;
		std::shared_ptr<Vector3> BlendNormals;
		std::shared_ptr<Vector4> BlendTangents;
		/** When JOINTS_0 is u8/u32, we store a converted copy (GPU path expects u16 indices). */
		std::shared_ptr<VertexBoneID> OwnedConvertedJointIds;
		/** When TEXCOORD_* is vec3/vec4 in glTF, GPU stream must be vec2 — owned XY copy (do not alias vec3 buffer as Vector2*). */
		std::shared_ptr<std::vector<Vector2>> OwnedTexCoords0;
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
			else if (attribute.first == "TEXCOORD_0" || attribute.first == "TEXCOORD_1")
			{
				if (attribute.first == "TEXCOORD_1" && d->Mesh->TextureCoords)
					continue;

				const int accIx = attribute.second;
				const auto& acc = d->Model->accessors[accIx];
				void* raw = Getdata(accIx, d->Mesh->nNumVertices, type);
				if (!raw)
					continue;

				const uint32_t nv = d->Mesh->nNumVertices;

				if (acc.componentType == TINYGLTF_COMPONENT_TYPE_FLOAT && acc.type == TINYGLTF_TYPE_VEC2)
				{
					d->Mesh->TextureCoords = static_cast<Vector2*>(raw);
					continue;
				}

				if (acc.componentType == TINYGLTF_COMPONENT_TYPE_FLOAT
					&& (acc.type == TINYGLTF_TYPE_VEC3 || acc.type == TINYGLTF_TYPE_VEC4))
				{
					const int comps = acc.type == TINYGLTF_TYPE_VEC3 ? 3 : 4;
					auto uvOwned = std::make_shared<std::vector<Vector2>>(nv);
					const float* p = static_cast<const float*>(raw);
					for (uint32_t i = 0; i < nv; ++i)
						(*uvOwned)[i] = Vector2(p[i * comps], p[i * comps + 1]);
					d->OwnedTexCoords0 = uvOwned;
					d->Mesh->TextureCoords = uvOwned->data();
					continue;
				}

				d->Mesh->TextureCoords = static_cast<Vector2*>(raw);
			}
			else if (attribute.first == "TANGENT")
			{
				d->Mesh->Tangents = (Vector4*)Getdata(attribute.second, d->Mesh->nNumVertices, type);
			}
			else if (attribute.first == "JOINTS_0")
			{
				int jointCompType = 0;
				void* rawJoints = Getdata(attribute.second, d->Mesh->nNumVertices, jointCompType);
				const uint32_t nv = d->Mesh->nNumVertices;
				if (jointCompType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE || jointCompType == TINYGLTF_COMPONENT_TYPE_BYTE)
				{
					const auto* src = static_cast<const uint8_t*>(rawJoints);
					d->OwnedConvertedJointIds.reset(new VertexBoneID[nv], [](VertexBoneID* p) { delete[] p; });
					for (uint32_t vi = 0; vi < nv; ++vi)
					{
						for (int k = 0; k < 4; ++k)
							d->OwnedConvertedJointIds.get()[vi].BoneIDs[k] = static_cast<uint16_t>(src[vi * 4u + static_cast<uint32_t>(k)]);
					}
					d->Mesh->BoneIDs = d->OwnedConvertedJointIds.get();
				}
				else if (jointCompType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT || jointCompType == TINYGLTF_COMPONENT_TYPE_INT)
				{
					const auto* src = static_cast<const uint32_t*>(rawJoints);
					d->OwnedConvertedJointIds.reset(new VertexBoneID[nv], [](VertexBoneID* p) { delete[] p; });
					for (uint32_t vi = 0; vi < nv; ++vi)
					{
						for (int k = 0; k < 4; ++k)
							d->OwnedConvertedJointIds.get()[vi].BoneIDs[k] = static_cast<uint16_t>(src[vi * 4u + static_cast<uint32_t>(k)]);
					}
					d->Mesh->BoneIDs = d->OwnedConvertedJointIds.get();
				}
				else
				{
					// UNSIGNED_SHORT / SHORT: 4 joints x 16-bit matches VertexBoneID layout
					d->Mesh->BoneIDs = static_cast<VertexBoneID*>(rawJoints);
				}
			}
			else if (attribute.first == "WEIGHTS_0")
			{
				d->Mesh->BoneWeights = (VertexBoneWeight*)Getdata(attribute.second, d->Mesh->nNumVertices, type);
			}
		}

		for (int i = 0; i < meshPrimitive.targets.size(); i++)
		{
			auto& target = meshPrimitive.targets[i];
			int type = 0;
			Vector3* BlendPosition = nullptr;
			Vector3* BlendNormal = nullptr;
			Vector3* BlendTangent = nullptr;
			for (const auto& attribute : target) {
				if (attribute.first == "POSITION")
				{
					BlendPosition = (Vector3*)Getdata(attribute.second, d->Mesh->nNumVertices, type);
				}
				else if (attribute.first == "NORMAL")
				{
					BlendNormal = (Vector3*)Getdata(attribute.second, d->Mesh->nNumVertices, type);
				}
				else if (attribute.first == "TANGENT")
				{
					BlendTangent = (Vector3*)Getdata(attribute.second, d->Mesh->nNumVertices, type);
				}

			}

			if (BlendPosition)
			{
				d->BlendShapes.push_back(BlendPosition);
				d->BlendShapeNormals.push_back(BlendNormal);
				d->BlendShapeTangents.push_back(BlendTangent);
			}
		}

		if (d->BlendShapes.size() > 0)
		{
			auto& BlendShapeName = d->Model->meshes[MeshIndex].extras.Get("targetNames");
			d->BlendShapeNames.resize(d->BlendShapes.size());
			for (int i = 0; i < d->BlendShapes.size(); i++)
			{
				std::string name = BlendShapeName.Get(i).Get<std::string>();
				std::string subName = name.substr(name.find('.') + 1);
				d->BlendShapeNames[i] = subName;
				//m_pBlendShapeName[i] = BlendShapeName.Get(i).Get<std::string>();
			}

		}

		int nMaterial = meshPrimitive.material >= 0 ? meshPrimitive.material : 0;
		d->Material = ModelMatrial[nMaterial];

		auto& Nodes = d->Model->nodes;
		d->NodeID = -1;
		d->SkinID = -1;
		const bool primitiveSkinned = (d->Mesh->BoneWeights != nullptr);
		const size_t primCount = d->Model->meshes[MeshIndex].primitives.size();

		std::vector<int> allMeshNodes;
		CollectMeshNodesSceneOrder(*d->Model, static_cast<int>(MeshIndex), allMeshNodes);
		if (allMeshNodes.empty())
		{
			allMeshNodes.reserve(Nodes.size());
			for (int i = 0; i < (int)Nodes.size(); ++i)
			{
				if (Nodes[i].mesh == (int)MeshIndex)
					allMeshNodes.push_back(i);
			}
		}

		std::vector<int> pickFrom = allMeshNodes;
		if (primitiveSkinned)
		{
			std::vector<int> skinnedNodes;
			for (int id : allMeshNodes)
			{
				if (Nodes[id].skin >= 0)
					skinnedNodes.push_back(id);
			}
			if (!skinnedNodes.empty())
				pickFrom = std::move(skinnedNodes);
		}

		if (!pickFrom.empty())
		{
			size_t pickIx = 0;
			if (pickFrom.size() == primCount)
				pickIx = PrimitiveIndex;
			else if (pickFrom.size() == 1)
				pickIx = 0;
			else
				pickIx = (std::min)(static_cast<size_t>(PrimitiveIndex), pickFrom.size() - 1);

			d->NodeID = pickFrom[pickIx];
			d->SkinID = Nodes[d->NodeID].skin;
			if (primitiveSkinned && d->SkinID < 0 && d->Model->skins.size() == 1)
				d->SkinID = 0;
		}

		if (d->NodeID >= 0)
		{
			auto& AllNodeInfos = ModelNode->GetAllNodes();
			if (d->NodeID < (int)AllNodeInfos.size())
				d->MeshMat = AllNodeInfos[d->NodeID]->FinalMeshMat;
		}

		d->MeshBuffer->InitMesh(d->Mesh);
	}

	bool GltfMesh::HasSkin() const
	{
		C_P(GltfMesh);
		return d->Mesh->BoneIDs != nullptr && d->Mesh->BoneWeights != nullptr;
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


	void GltfMesh::GenVertWithWeights(const std::vector<float>& weight)
	{
		C_P(GltfMesh);
		if (d->BlendShapes.size() == 0 || weight.empty())
		{
			return;
		}

		if (d->BlendVerts == NULL)
		{
			d->BlendVerts.reset(new Vector3[d->Mesh->nNumVertices], [](Vector3* p) {delete[]p; });
		}
		if (d->Mesh->Normals && d->BlendNormals == NULL)
		{
			d->BlendNormals.reset(new Vector3[d->Mesh->nNumVertices], [](Vector3* p) {delete[]p; });
		}
		if (d->Mesh->Tangents && d->BlendTangents == NULL)
		{
			d->BlendTangents.reset(new Vector4[d->Mesh->nNumVertices], [](Vector4* p) {delete[]p; });
		}

		memcpy(d->BlendVerts.get(), d->Mesh->Vertices, sizeof(Vector3) * d->Mesh->nNumVertices);
		if (d->BlendNormals)
		{
			memcpy(d->BlendNormals.get(), d->Mesh->Normals, sizeof(Vector3) * d->Mesh->nNumVertices);
		}
		if (d->BlendTangents)
		{
			memcpy(d->BlendTangents.get(), d->Mesh->Tangents, sizeof(Vector4) * d->Mesh->nNumVertices);
		}

		size_t BlendShapeCount = (std::min)(d->BlendShapes.size(), weight.size());
		for (size_t i = 0; i < BlendShapeCount; i++)
		{
			float w = weight[i];
			Vector3* pBlendShape = d->BlendShapes[i];
			Vector3* pBlendNormal = d->BlendShapeNormals[i];
			Vector3* pBlendTangent = d->BlendShapeTangents[i];
			if (w < 0.001 && w>-0.001)
			{
				continue;
			}
			for (int j = 0; j < d->Mesh->nNumVertices; j++)
			{
				d->BlendVerts.get()[j] += w * pBlendShape[j];
				if (d->BlendNormals && pBlendNormal)
				{
					d->BlendNormals.get()[j] += w * pBlendNormal[j];
				}
				if (d->BlendTangents && pBlendTangent)
				{
					d->BlendTangents.get()[j].x += w * pBlendTangent[j].x;
					d->BlendTangents.get()[j].y += w * pBlendTangent[j].y;
					d->BlendTangents.get()[j].z += w * pBlendTangent[j].z;
				}
			}
		}
		d->MeshBuffer->UpdateVert(d->BlendVerts.get(), d->Mesh->nNumVertices);
		if (d->BlendNormals)
		{
			for (int j = 0; j < d->Mesh->nNumVertices; j++)
			{
				d->BlendNormals.get()[j].Normalize();
			}
			d->MeshBuffer->UpdateNormal(d->BlendNormals.get(), d->Mesh->nNumVertices);
		}
		if (d->BlendTangents)
		{
			for (int j = 0; j < d->Mesh->nNumVertices; j++)
			{
				Vector3 Tangent(d->BlendTangents.get()[j].x, d->BlendTangents.get()[j].y, d->BlendTangents.get()[j].z);
				Tangent.Normalize();
				d->BlendTangents.get()[j].x = Tangent.x;
				d->BlendTangents.get()[j].y = Tangent.y;
				d->BlendTangents.get()[j].z = Tangent.z;
			}
			d->MeshBuffer->UpdateTangent(d->BlendTangents.get(), d->Mesh->nNumVertices);
		}
	}

}
