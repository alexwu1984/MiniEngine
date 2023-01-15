#pragma once
#include "inc.h"

namespace Engine
{
	struct GltfModelP;
	class GltfMaterial;
	class GltfNode;
	class GltfMesh;

	class GltfModel
	{
	public:
		GltfModel();
		~GltfModel();

		bool Load(const std::wstring& FileName);
		std::vector<std::shared_ptr<GltfMesh>>& GetModelMesh();
	private:
		void LoadNode();
		void LoadMesh();
		std::vector <std::shared_ptr<GltfMaterial>> LoadMaterial();
	private:
		std::shared_ptr< GltfModelP> Data;
	};
}