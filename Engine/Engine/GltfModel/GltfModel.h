#pragma once
#include "math/aabb3.h"

namespace Engine
{
	struct GltfModelPrivate;
	class GltfMaterial;
	class GltfNode;
	class GltfMesh;
	class GltfSkeleton;
	class GltfModelConfig;

	class GltfModel : public std::enable_shared_from_this<GltfModel>
	{
	public:
		GltfModel();
		~GltfModel();

		bool Load(const std::wstring& FileName, std::shared_ptr< GltfModelConfig> Config);
		std::vector<std::shared_ptr<GltfMesh>>& GetModelMesh();
		math::AABB3 GetModelBox() const;
		std::shared_ptr<GltfNode> RootNode();
		std::shared_ptr<GltfSkeleton> GetSkeleton();
		void Play(float TotalDeltaTime,float DeltaFrameTime);
		std::shared_ptr< GltfModelConfig> GetModelConfig() const;
	private:
		void LoadNode();
		void UpdateNode();
		void LoadMesh();
		void LoadAnimate();
		void LoadSkeleton();

		std::vector <std::shared_ptr<GltfMaterial>> LoadMaterial();
	private:
		GltfModelPrivate* d_ptr = nullptr;
	};
}