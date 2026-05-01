#include "ObjModel/ObjModel.h"
#include "ObjModel/ObjMesh.h"
#include "GltfModel/GltfModelConfig.h"
#include "core/strings.h"
#include <Assimp/Importer.hpp>
#include <Assimp/scene.h>
#include <Assimp/postprocess.h>
#include "ObjModel/ObjMesh.h"

namespace Engine
{
	using namespace math;

	struct ObjModelPrivate
	{
		Assimp::Importer ModelImpoter;
		const aiScene* pScene = nullptr;
		std::string Directory;
		std::vector<std::shared_ptr<ObjMesh>> ModelMesh;
		AABB3  ModelBox;
	};

	ObjModel::ObjModel()
		:d_ptr(new ObjModelPrivate())
	{

	}

	ObjModel::~ObjModel()
	{
		delete d_ptr;
	}

	bool ObjModel::Load(const std::wstring& FileName, std::shared_ptr<GltfModelConfig> Config)
	{
		C_P(ObjModel);
		std::string utf8FileName = core::ucs2_u8(FileName);
		d->pScene = d->ModelImpoter.ReadFile(utf8FileName.c_str(), aiProcess_Triangulate | aiProcess_CalcTangentSpace | aiProcess_FlipWindingOrder | aiProcess_FlipUVs);
		if (!d->pScene || !d->pScene->mRootNode || d->pScene->mFlags == AI_SCENE_FLAGS_INCOMPLETE)
		{
			return false;
		}
		d->Directory = utf8FileName.substr(0, utf8FileName.find_last_of('/'));
		traverseNodes();
		return true;
	}

	std::vector<std::shared_ptr<ObjMesh>>& ObjModel::GetModelMesh()
	{
		C_P(ObjModel);
		return d->ModelMesh;
	}

	math::AABB3 ObjModel::GetModelBox() const
	{
		C_P(ObjModel);
		return d->ModelBox;
	}

	void ObjModel::traverseNodes()
	{
		C_P(ObjModel);
		int32_t NumMeshes = d->pScene->mNumMeshes;
		for (int32_t i = 0; i < NumMeshes; ++i)
		{
			std::shared_ptr<ObjMesh> mesh = std::make_shared<ObjMesh>(d->pScene, d->pScene->mMeshes[i],d->Directory);
			mesh->Init();
			d->ModelMesh.push_back(mesh);

			AABB3 TmpMeshBox = mesh->GetBoundingBox();
			d->ModelBox.UpdateMinMax(TmpMeshBox.GetMinPoint());
			d->ModelBox.UpdateMinMax(TmpMeshBox.GetMaxPoint());
		}
	}

}