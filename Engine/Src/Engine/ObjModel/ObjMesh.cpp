#include "ObjModel/ObjMesh.h"
#include "GltfModel/GltfMeshInfo.h"
#include "GltfModel/GltfMeshBuffer.h"
#include "Material/ObjMatereial.h"
#include <Assimp/Importer.hpp>
#include <Assimp/scene.h>
#include <Assimp/postprocess.h>

namespace Engine
{
	struct ObjMeshPrivate
	{
		const aiScene* pScene = nullptr;
		aiMesh* vAiMesh = nullptr;
		std::shared_ptr<GltfMeshInfo> Mesh;
		std::shared_ptr<GltfMeshBuffer> MeshBuffer;
		std::string MeshName;
		std::string Directory;

		std::vector<math::Vector3> Positions;
		std::vector<math::Vector3> Normals;
		std::vector<math::Vector2> TexCoords;
		std::vector<math::Vector3> Tangents;
		std::vector<int32_t> Indices;
	};

	ObjMesh::ObjMesh(const aiScene* pScene, aiMesh* pMesh, const std::string& Directory)
		:d_ptr(new ObjMeshPrivate())
	{
		C_P(ObjMesh);
		d->pScene = pScene;
		d->vAiMesh = pMesh;
		d->Directory = Directory;
		d->Mesh = std::make_shared<GltfMeshInfo>();
		d->MeshBuffer = std::make_shared<GltfMeshBuffer>();
	}

	ObjMesh::~ObjMesh()
	{
		delete d_ptr;
	}

	void ObjMesh::Init()
	{
		ProcessVertex();
		ProcessIndices();
	}

	void ObjMesh::ProcessVertex()
	{
		C_P(ObjMesh);
		int32_t NumVertices = (int32_t)d->vAiMesh->mNumVertices;
		for (int32_t i = 0; i < NumVertices; ++i)
		{
			d->Positions.emplace_back(math::Vector3(d->vAiMesh->mVertices[i].x, d->vAiMesh->mVertices[i].y, d->vAiMesh->mVertices[i].z));
			math::Vector3 Normal;
			if (d->vAiMesh->mNormals != nullptr)
				Normal = math::Vector3(d->vAiMesh->mNormals[i].x, d->vAiMesh->mNormals[i].y, d->vAiMesh->mNormals[i].z);
			d->Normals.emplace_back(Normal);
			math::Vector2 TextureCoord;
			if (d->vAiMesh->mTextureCoords[0])
				TextureCoord = math::Vector2(d->vAiMesh->mTextureCoords[0][i].x, d->vAiMesh->mTextureCoords[0][i].y);
			d->TexCoords.emplace_back(TextureCoord);
			math::Vector3 Tangent;
			if (d->vAiMesh->mTangents)
				Tangent = math::Vector3(d->vAiMesh->mTangents[i].x, d->vAiMesh->mTangents[i].y, d->vAiMesh->mTangents[i].z);
			d->Tangents.emplace_back(Tangent);
		}
		d->Mesh->nNumVertices = NumVertices;
		d->Mesh->Vertices = d->Positions.data();
	}

	void ObjMesh::ProcessIndices()
	{
		C_P(ObjMesh);
		int32_t NumFaces = d->vAiMesh->mNumFaces;
		for (int32_t i = 0; i < NumFaces; ++i)
		{
			aiFace AiFace = d->vAiMesh->mFaces[i];
			int32_t NumIndices = AiFace.mNumIndices;
			for (int32_t k = 0; k < NumIndices; ++k)
				d->Indices.push_back(AiFace.mIndices[k]);
		}
	}

}