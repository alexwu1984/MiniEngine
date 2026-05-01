#pragma once
#include "core/inc.h"
#include "math/aabb3.h"
#include "GltfModel/GltfModelBase.h"
#include "GltfModel/MeshBase.h"
#include "GltfModel/DynamicBoneInfo.h"

namespace Engine
{
	struct GltfMeshPrivate;
	class GltfMaterial;
	class GltfMeshBuffer;
	class GltfNode;
	class GltfModel;

	class GltfMesh :public MeshBase, public GltfModelBase
	{
	public:
		GltfMesh(tinygltf::Model* Model, GltfModel* Owner);
		~GltfMesh();

		void Init(uint32_t MeshIndex, uint32_t PrimitiveIndex, const std::vector < std::shared_ptr<GltfMaterial>>& ModelMatrial, std::shared_ptr< GltfNode> ModelNode);
		virtual bool HasSkin() const override;
		virtual const math::AABB3& GetBoundingBox() const override;
		virtual const math::Matrix4x4& GetMeshMat() const override;
		virtual std::shared_ptr<GltfMeshBuffer> GetMeshBuffer() override;
		virtual std::shared_ptr<MaterialBase> GetMaterial() override;
		virtual std::string GetMeshName() const override;
		virtual int32_t GetNodeId() const override;
		virtual int32_t GetSkinId() const override;
		void SetMeshMat(const math::Matrix4x4& Mat);
		virtual std::vector<std::vector<BoneSkinInfo>>& GetBoneNodeArray() override;
		void GenVertWithWeights(const std::vector<float>& weight);

	private:
		GltfMeshPrivate* d_ptr;
	};
}