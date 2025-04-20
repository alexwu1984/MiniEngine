#pragma once
#include "math/aabb3.h"

namespace Engine
{
	class GltfModelConfig;
	class ObjMesh;
	struct ObjModelPrivate;

	class ObjModel : public std::enable_shared_from_this<ObjModel>
	{
	public:
		ObjModel();
		~ObjModel();

		bool Load(const std::wstring& FileName, std::shared_ptr<GltfModelConfig> Config);
		std::vector<std::shared_ptr<ObjMesh>>& GetModelMesh();
		math::AABB3 GetModelBox() const;
	private:
		void traverseNodes();
	private:
		ObjModelPrivate* d_ptr = nullptr;
	};
}