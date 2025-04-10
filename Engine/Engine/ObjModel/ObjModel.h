#pragma once
#include "math/aabb3.h"

namespace Engine
{
	class GltfModelConfig;
	struct ObjModelPrivate;

	class ObjModel : public std::enable_shared_from_this<ObjModel>
	{
	public:
		ObjModel();
		~ObjModel();

		bool Load(const std::wstring& FileName, std::shared_ptr<GltfModelConfig> Config);
	private:
		void traverseNodes();
	private:
		ObjModelPrivate* d_ptr = nullptr;
	};
}