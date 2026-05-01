#include "AssimpModel/AssimpModel.h"
#include "AssimpModel/AssimpMesh.h"
#include "Scene/SceneModelAsset.h"
#include "core/strings.h"
#include <filesystem>
#include <Assimp/Importer.hpp>
#include <Assimp/scene.h>
#include <Assimp/postprocess.h>

namespace Engine
{
	using namespace math;

	struct AssimpModelPrivate
	{
		Assimp::Importer ModelImpoter;
		const aiScene* pScene = nullptr;
		std::string Directory;
		std::vector<std::shared_ptr<AssimpMesh>> ModelMesh;
		AABB3  ModelBox;
	};

	AssimpModel::AssimpModel()
		:d_ptr(new AssimpModelPrivate())
	{

	}

	AssimpModel::~AssimpModel()
	{
		delete d_ptr;
	}

	bool AssimpModel::Load(const std::wstring& FileName, std::shared_ptr<SceneModelAsset> Asset)
	{
		C_P(AssimpModel);
		std::string utf8FileName = core::ucs2_u8(FileName);
		// Engine default raster state treats CW as front-face (see RHICachedStates::RasterizerStateCullBack = CM_CW).
		// Flipping winding here would invert faces and break back-face culling.
		d->pScene = d->ModelImpoter.ReadFile(utf8FileName.c_str(), aiProcess_Triangulate | aiProcess_CalcTangentSpace | aiProcess_FlipUVs);
		if (!d->pScene || !d->pScene->mRootNode || d->pScene->mFlags == AI_SCENE_FLAGS_INCOMPLETE)
		{
			return false;
		}
		// Robust directory extraction on Windows (paths may contain '\\').
		// Assimp uses the directory to resolve MTL/texture relative paths.
		// Note: u8string() returns std::u8string in C++20; convert to std::string.
		d->Directory = core::ucs2_u8(std::filesystem::path(utf8FileName).parent_path().wstring());
		traverseNodes();
		return true;
	}

	std::vector<std::shared_ptr<AssimpMesh>>& AssimpModel::GetModelMesh()
	{
		C_P(AssimpModel);
		return d->ModelMesh;
	}

	math::AABB3 AssimpModel::GetModelBox() const
	{
		C_P(AssimpModel);
		return d->ModelBox;
	}

	void AssimpModel::traverseNodes()
	{
		C_P(AssimpModel);
		int32_t NumMeshes = d->pScene->mNumMeshes;
		for (int32_t i = 0; i < NumMeshes; ++i)
		{
			std::shared_ptr<AssimpMesh> mesh = std::make_shared<AssimpMesh>(d->pScene, d->pScene->mMeshes[i], d->Directory);
			mesh->Init();
			d->ModelMesh.push_back(mesh);

			AABB3 TmpMeshBox = mesh->GetBoundingBox();
			d->ModelBox.UpdateMinMax(TmpMeshBox.GetMinPoint());
			d->ModelBox.UpdateMinMax(TmpMeshBox.GetMaxPoint());
		}
	}

}