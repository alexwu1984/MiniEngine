#pragma once
#include "math/aabb3.h"

namespace Engine
{
	class SceneModelAsset;
	class AssimpMesh;
	struct AssimpModelPrivate;

	// Assimp-backed static model (OBJ/FBX/...). Kept separate from GltfModel.
	class AssimpModel : public std::enable_shared_from_this<AssimpModel>
	{
	public:
		AssimpModel();
		~AssimpModel();

		bool Load(const std::wstring& FileName, std::shared_ptr<SceneModelAsset> Asset);
		std::vector<std::shared_ptr<AssimpMesh>>& GetModelMesh();
		math::AABB3 GetModelBox() const;
	private:
		void traverseNodes();
	private:
		AssimpModelPrivate* d_ptr = nullptr;
	};
}