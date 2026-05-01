#pragma once
#include "GltfModel/GltfSkeleton.h"
#include "GltfModel/DyTransfromNode.h"

namespace Engine
{
	struct DynamicBoneManagerPrivate;

	class FDynamicBoneManager
	{
	public:
		FDynamicBoneManager();
		~FDynamicBoneManager();

		void InitParticle(std::shared_ptr<FDyTransformNode> TransNode, int32_t BoneIndex = -1);
		void InitTransfrom(int32_t BoneIndex = -1);
		std::shared_ptr<FDyTransformNode> GetTransformNode(const std::string& NodeName);
		
		void DFSInitDynamicBoneNode(std::shared_ptr<GltfBoneNodeInfo> NodeInfo, const math::Matrix4x4& ParentMatrix);

		void DynamicBonePreUpdate(std::shared_ptr<GltfBoneNodeInfo> NodeInfo, const math::Matrix4x4& ParentMatrix);

		void LateUpdate(int BoneIndex = -1);
		void ResetDynamicBone();

		void DeleteDynamicBone(const std::string& BoneName);
		void DeleteChildTransfromNode(const std::string& BoneName);
		void UpdateDynamicBoneParameter(const FDynamicBoneInfo& param);
		
		void AddBoneParam(const FDynamicBoneInfo& BoneInfo);

	private:
		DynamicBoneManagerPrivate* Impl = nullptr;
	};
}