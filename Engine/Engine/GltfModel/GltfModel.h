#pragma once
#include "math/aabb3.h"

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
		void UpdateNode();
		std::vector<std::shared_ptr<GltfMesh>>& GetModelMesh();
		math::AABB3 GetModelBox() const;
	private:
		void LoadNode();
		void LoadMesh();
		void LoadAnimate();

		std::vector <std::shared_ptr<GltfMaterial>> LoadMaterial();
	private:
		std::shared_ptr< GltfModelP> Impl;
	};
}