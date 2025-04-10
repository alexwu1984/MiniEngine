#pragma once
#include "Material/MaterialBase.h"
#include <Assimp/material.h>

struct aiMesh;
struct aiScene;

namespace Engine
{
	struct ObjMaterialPrivate;

	class ObjMaterial : public MaterialBase
	{
	public:
		ObjMaterial(aiScene* pScene, aiMesh* pMesh, const std::string& Directory);
		~ObjMaterial();
		void Init();
	private:
		void loadTextureFromMaterial(aiTextureType vTextureType, const aiMaterial* vMat, std::map< int32_t, aiString> &TexNames);
	private:
		ObjMaterialPrivate* d_ptr = nullptr;
	};
}