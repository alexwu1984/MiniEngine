#pragma once
#include "GltfModel/GltfSkeleton.h"
#include "GltfModel/DyTransfromNode.h"

namespace Engine
{
	struct DynamicBoneManagerP;

	class DynamicBoneManager
	{
	public:
		DynamicBoneManager();
		~DynamicBoneManager();

		//初始化动态骨骼所持有的transfrom接口，记录位置信息与转换矩阵
		//param: BoneIndex  动态骨骼索引，为 - 1时表示没有动态骨骼
		void InitParticle(std::shared_ptr<DyTransformNode> TransNode, int32_t BoneIndex = -1);
		//动态骨骼transfrom局部坐标初始化
		//param: BoneIndex  动态骨骼索引，为 - 1时表示没有动态骨骼
		void InitTransfrom(int32_t BoneIndex = -1);
		std::shared_ptr<DyTransformNode> GetTransformNode(const std::string& NodeName);
		
		//递归初始化动态骨骼各节点，初始化每个transform接口
		//param: GltfBoneNodeInfo  骨骼信息
		void DFSInitDynamicBoneNode(std::shared_ptr<GltfBoneNodeInfo> NodeInfo, const math::Matrix4x4& ParentMatrix);

		//动态骨骼前更新，将动画每一帧的变化都同步到动态骨骼的transform接口
		//param: CC3DBoneNodeInfo  骨骼信息
		void DynamicBonePreUpdate(std::shared_ptr<GltfBoneNodeInfo> NodeInfo, const math::Matrix4x4& ParentMatrix);

		//动态骨骼后更新，等待骨骼动画动作完成之后更新动态骨骼位置
		//param: BoneIndex  动态骨骼索引，为-1时表示没有动态骨骼
		void LateUpdate(int BoneIndex = -1);
		void ResetDynamicBone();

		void DeleteDynamicBone(const std::string& BoneName);
		void DeleteChildTransfromNode(const std::string& BoneName);
		void UpdateDynamicBoneParameter(const DynamicBoneInfo& param);
		
		void AddBoneParam(const DynamicBoneInfo& BoneInfo);

	private:
		DynamicBoneManagerP* Impl = nullptr;
	};
}