#pragma once
#include "GltfModel/GltfModelBase.h"
#include "GltfModel/DynamicBoneInfo.h"

namespace Engine
{
	class GltfNode;
	class DynamicBoneManager;
	struct GltfSkeletonPrivate;

	class GltfSkeleton : public GltfModelBase
	{
	public:
		GltfSkeleton(tinygltf::Model* gltfModel, std::shared_ptr<GltfNode> Node);
		virtual ~GltfSkeleton();

		void InitSkeleton();
		void UpdateBone();
		void UseInitPos();
		void AddDynamicBone(const std::vector<DynamicBoneInfo>& BoneInfoArray);
		void ResetDynamicBone();
		void DeleteDynamicBone(const std::string& BoneName);
		void UpdateDynamicBoneParameter(const DynamicBoneInfo& param);
		bool HasDynamicBone() const;
		std::vector<std::vector<BoneSkinInfo>>& GetBoneNodeArray();
		std::vector<std::shared_ptr<GltfBoneNodeInfo>>& GetRootNode();

	private:
		void CreateModelBoneTree(int32_t NodeID);
		//递归更新骨骼最终状态，，在此处进行动态骨骼更新，同步动态骨骼信息，计算出最后的变化矩阵
		//param: CC3DBoneNodeInfo  骨骼信息
		void DFSBoneTree(std::shared_ptr< GltfBoneNodeInfo> BoneNodeInfo, math::Matrix4x4& ParentMatrix);
		void InitDynamicBoneNode();

	private:
		GltfSkeletonPrivate* Impl = nullptr;

	};
}