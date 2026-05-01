#include "AssimpModel/AssimpMesh.h"
#include "GltfModel/GltfMeshInfo.h"
#include "GltfModel/GltfMeshBuffer.h"
#include "GltfModel/DynamicBoneInfo.h"
#include "Material/AssimpMaterial.h"
#include <Assimp/Importer.hpp>
#include <Assimp/scene.h>
#include <Assimp/postprocess.h>


namespace Engine
{
	struct AssimpMeshPrivate
	{
		const aiScene* pScene = nullptr;
		aiMesh* vAiMesh = nullptr;
		std::shared_ptr<GltfMeshInfo> Mesh;
		std::shared_ptr<GltfMeshBuffer> MeshBuffer;
		std::shared_ptr<AssimpMaterial> Material;
		std::string MeshName;
		std::string Directory;

		std::vector<math::Vector3> Positions;
		std::vector<math::Vector3> Normals;
		std::vector<math::Vector2> TexCoords;
		std::vector<math::Vector4> Tangents;
		std::vector<uint32_t> Indices;

		math::AABB3  ModelBox;
		math::Matrix4x4 MeshMat;
		std::vector<std::vector<BoneSkinInfo>> BoneSkinInfos;
	};

	AssimpMesh::AssimpMesh(const aiScene* pScene, aiMesh* pMesh, const std::string& Directory)
		:d_ptr(new AssimpMeshPrivate())
	{
		C_P(AssimpMesh);
		d->pScene = pScene;
		d->vAiMesh = pMesh;
		d->Directory = Directory;
		d->Mesh = std::make_shared<GltfMeshInfo>();
		d->MeshBuffer = std::make_shared<GltfMeshBuffer>();
	}

	AssimpMesh::~AssimpMesh()
	{
		delete d_ptr;
	}

	void AssimpMesh::Init()
	{
		C_P(AssimpMesh);
		ProcessVertex();
		ProcessIndices();
		ProcessTextures();
		d->MeshBuffer->InitMesh(d->Mesh);
	}

	const math::AABB3& AssimpMesh::GetBoundingBox() const
	{
		C_P(AssimpMesh);
		return d->ModelBox;
	}

	const math::Matrix4x4& AssimpMesh::GetMeshMat() const
	{
		C_P(AssimpMesh);
		return d->MeshMat;
	}

	std::shared_ptr<GltfMeshBuffer> AssimpMesh::GetMeshBuffer()
	{
		C_P(AssimpMesh);
		return d->MeshBuffer;
	}

	std::shared_ptr<Engine::MaterialBase> AssimpMesh::GetMaterial()
	{
		C_P(AssimpMesh);
		return d->Material;
	}

	std::string AssimpMesh::GetMeshName() const
	{
		C_P(AssimpMesh);
		return d->MeshName;
	}

	std::vector<std::vector<BoneSkinInfo>>& AssimpMesh::GetBoneNodeArray()
	{
		C_P(AssimpMesh);
		return d->BoneSkinInfos;
	}

	void AssimpMesh::ProcessVertex()
	{
		C_P(AssimpMesh);
		int32_t NumVertices = (int32_t)d->vAiMesh->mNumVertices;
		for (int32_t i = 0; i < NumVertices; ++i)
		{
			d->Positions.emplace_back(math::Vector3(d->vAiMesh->mVertices[i].x, d->vAiMesh->mVertices[i].y, d->vAiMesh->mVertices[i].z));
			math::Vector3 Normal{ 0.0f, 0.0f, 1.0f };
			if (d->vAiMesh->mNormals != nullptr)
				Normal = math::Vector3(d->vAiMesh->mNormals[i].x, d->vAiMesh->mNormals[i].y, d->vAiMesh->mNormals[i].z);
			d->Normals.emplace_back(Normal);
			math::Vector2 TextureCoord{ 0.0f, 0.0f };
			if (d->vAiMesh->mTextureCoords[0])
				TextureCoord = math::Vector2(d->vAiMesh->mTextureCoords[0][i].x, d->vAiMesh->mTextureCoords[0][i].y);
			d->TexCoords.emplace_back(TextureCoord);
			math::Vector4 Tangent{ 1.0f, 0.0f, 0.0f, 1.0f };
			if (d->vAiMesh->mTangents)
				Tangent = math::Vector4(d->vAiMesh->mTangents[i].x, d->vAiMesh->mTangents[i].y, d->vAiMesh->mTangents[i].z,1.0f);
			d->Tangents.emplace_back(Tangent);
		}
		d->Mesh->nNumVertices = NumVertices;
		d->Mesh->Vertices = d->Positions.data();
		d->Mesh->Normals = d->Normals.data();
		d->Mesh->TextureCoords = d->TexCoords.data();
		d->Mesh->Tangents = d->Tangents.data();
		d->ModelBox.CreateAABB(d->Positions);
	}

	void AssimpMesh::ProcessIndices()
	{
		C_P(AssimpMesh);
		int32_t NumFaces = d->vAiMesh->mNumFaces;
		for (int32_t i = 0; i < NumFaces; ++i)
		{
			aiFace AiFace = d->vAiMesh->mFaces[i];
			int32_t NumIndices = AiFace.mNumIndices;
			for (int32_t k = 0; k < NumIndices; ++k)
				d->Indices.push_back(AiFace.mIndices[k]);
		}
		d->Mesh->nNumFaces = NumFaces;
		d->Mesh->FacesIndex32 = d->Indices.data();
	}

	void AssimpMesh::ProcessTextures()
	{
		C_P(AssimpMesh);
		d->Material = std::make_shared<AssimpMaterial>(d->pScene, d->vAiMesh, d->Directory);
		d->Material->Init();
	}

}