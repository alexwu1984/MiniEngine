#pragma once
#include "GltfModel/GltfModelBase.h"
#include "GltfModel/DynamicBoneInfo.h"

namespace Engine
{
	class GltfNode;

	class GltfSkeleton : public GltfModelBase
	{
	public:
		GltfSkeleton(tinygltf::Model* gltfModel, std::shared_ptr<GltfNode> Node);
		virtual ~GltfSkeleton();

		void InitSkeleton();
		void UpdateBone();
		void UseInitPos();
		void UpdateNeckBone(std::vector<float>& headRotation, const char* neckBoneName);
		void AddDynamicBone(const std::vector<DynamicBoneParameter>& db_paramter_array);
		void ResetDynamicBone();
		void DeleteDynamicBone(const std::string& db_name);
		void UpdateDynamicBoneParameter(const DynamicBoneParameter& param);
		bool HasDynamicBone() const;

	private:
		void CreateModelBoneTree(int32_t NodeID);
		/*
 @递归更新骨骼最终状态，，在此处进行动态骨骼更新，同步动态骨骼信息，计算出最后的变化矩阵
 @param: CC3DBoneNodeInfo  骨骼信息
*/
		void DFSBoneTree(std::shared_ptr< GltfBoneNodeInfo> BoneNodeInfo, math::Matrix4x4& ParentMatrix);
	private:
		std::vector<std::shared_ptr<GltfBoneNodeInfo>>	_BoneNode;
		std::vector<std::vector<BoneSkinInfo>>			_BoneNodeArray;

		//骨骼名对应的骨骼ID
		std::map<std::string, int32_t>					_BoneMap;
		//NodeID对应的骨骼ID
		std::map<int32_t, int32_t>						_NodeBoneMap;
		std::vector<std::shared_ptr<GltfBoneNodeInfo>>	_RootNode;

		std::shared_ptr<GltfNode>						_ModelNode;
		int32_t											_BoneIndex = 0;
	};
}