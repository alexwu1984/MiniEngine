#pragma once
#include "core/inc.h"
#include "math/aabb3.h"
#include "GltfModel/GltfModelBase.h"
#include "GltfModel/DynamicBoneInfo.h"

namespace Engine
{
	struct GltfMeshP;
	class GltfMaterial;
	class GltfMeshBuffer;
	class GltfNode;
	class GltfModel;

	class GltfMesh : public GltfModelBase
	{
	public:
		GltfMesh(tinygltf::Model* Model, GltfModel* Owner);
		~GltfMesh();

		void Init(uint32_t MeshIndex, uint32_t PrimitiveIndex, const std::vector < std::shared_ptr<GltfMaterial>>& ModelMatrial, std::shared_ptr< GltfNode> ModelNode);
		bool HasSkin() const;
		const math::AABB3& GetBoundingBox() const;
		const math::Matrix4x4& GetMeshMat() const;
		std::shared_ptr<GltfMeshBuffer> GetMeshBuffer();
		std::shared_ptr<GltfMaterial> GetMaterial();
		std::string GetMeshName() const;
		int32_t GetNodeId() const;
		int32_t GetSkinId() const;
		void SetMeshMat(const math::Matrix4x4& Mat);
		std::vector<std::vector<BoneSkinInfo>>& GetBoneNodeArray();

	private:
		std::shared_ptr< GltfMeshP> Impl;
	};
}