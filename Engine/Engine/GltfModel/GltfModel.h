#pragma once
#include "math/aabb3.h"

namespace Engine
{
	struct GltfModelP;
	class GltfMaterial;
	class GltfNode;
	class GltfMesh;
	class GltfSkeleton;

	class GltfModel
	{
	public:
		GltfModel();
		~GltfModel();

		bool Load(const std::wstring& FileName);
		void UpdateNode();
		std::vector<std::shared_ptr<GltfMesh>>& GetModelMesh();
		math::AABB3 GetModelBox() const;
		std::shared_ptr<GltfNode> RootNode();
		std::shared_ptr<GltfSkeleton> GetSkeleton();
		void Play(float TotalDeltaTime,float DeltaFrameTime);
	private:
		void LoadNode();
		void LoadMesh();
		void LoadAnimate();
		void LoadSkeleton();

		std::vector <std::shared_ptr<GltfMaterial>> LoadMaterial();
	private:
		std::shared_ptr< GltfModelP> Impl;
	};
}