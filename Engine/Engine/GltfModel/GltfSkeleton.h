#pragma once
#include "GltfModel/GltfModelBase.h"
#include "GltfModel/DynamicBoneInfo.h"

namespace Engine
{
	class GltfNode;
	class FDynamicBoneManager;
	struct GltfSkeletonPrivate;

	class GltfSkeleton : public GltfModelBase
	{
	public:
		GltfSkeleton(tinygltf::Model* gltfModel, std::shared_ptr<GltfNode> Node);
		virtual ~GltfSkeleton();

		void InitSkeleton();
		void UpdateBone();
		void UseInitPos();
		void AddDynamicBone(const std::vector<FDynamicBoneInfo>& BoneInfoArray);
		void ResetDynamicBone();
		void DeleteDynamicBone(const std::string& BoneName);
		void UpdateDynamicBoneParameter(const FDynamicBoneInfo& param);
		bool HasDynamicBone() const;
		std::vector<std::vector<BoneSkinInfo>>& GetBoneNodeArray();
		std::vector<std::shared_ptr<GltfBoneNodeInfo>>& GetRootNode();
		std::shared_ptr<GltfBoneNodeInfo> GetBoneNodeByNodeId(int32_t NodeID) const;

	private:
		void CreateModelBoneTree(int32_t NodeID);
		void DFSBoneTree(std::shared_ptr< GltfBoneNodeInfo> BoneNodeInfo, math::Matrix4x4& ParentMatrix);
		void InitDynamicBoneNode();

	private:
		GltfSkeletonPrivate* d_ptr = nullptr;

	};
}