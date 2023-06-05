#include "GltfModel/DynamicBoneManager.h"
#include "GltfModel/DynamicBone.h"
#include "GltfModel/DyTransfromNode.h"

using namespace math;

namespace Engine
{
	struct DynamicBoneManagerP
	{
		//动态骨骼各个particle的transform接口
		std::unordered_map<std::string, std::shared_ptr<DyTransformNode>>		TransfromMap;
		//动态骨骼配置项参数
		std::vector<DynamicBoneInfo>					DyBoneInfoArray;
		//动态骨骼列表
		std::vector<std::shared_ptr<DynamicBone>>			DyBoneArray;
		//动态骨骼碰撞体
		//std::vector<DynamicBoneColliderBase*>				colliderArray;
	};

	DynamicBoneManager::DynamicBoneManager()
		:Impl(new DynamicBoneManagerP())
	{

	}

	DynamicBoneManager::~DynamicBoneManager()
	{

		delete Impl;
	}

	void DynamicBoneManager::InitParticle(std::shared_ptr<DyTransformNode> TransNode, int32_t BoneIndex /*= -1*/)
	{
		if (TransNode == nullptr || BoneIndex == -1 || BoneIndex >= Impl->DyBoneArray.size())
		{
			return;
		}

		Impl->DyBoneArray[BoneIndex]->InitParticle(TransNode.get());
	}

	void DynamicBoneManager::InitTransfrom(int32_t BoneIndex /*= -1*/)
	{
		if (BoneIndex == -1 || BoneIndex >= Impl->DyBoneArray.size())
		{
			return;
		}

		Impl->DyBoneArray[BoneIndex]->InitTransform();
	}

	std::shared_ptr<Engine::DyTransformNode> DynamicBoneManager::GetTransformNode(const std::string& BoneName)
	{
		auto Iter = Impl->TransfromMap.find(BoneName);
		if (Iter != Impl->TransfromMap.end())
		{
			return Iter->second;
		}
		return nullptr;
	}

	void DynamicBoneManager::DFSInitDynamicBoneNode(std::shared_ptr<GltfBoneNodeInfo> NodeInfo, const math::Matrix4x4& ParentMatrix)
	{
		Matrix4x4 mat4Scaling = Matrix4x4::ScaleMatrix(Vector3(NodeInfo->TargetScale.x, NodeInfo->TargetScale.y, NodeInfo->TargetScale.z));
		Matrix4x4 mat4Rotation = Matrix4x4::CreateFromQuaternion(Quaternion(NodeInfo->TargetRotation));
		Matrix4x4 mat4Translation = Matrix4x4::CreateFromTranslate(Vector3(NodeInfo->TargetTranslate.x, NodeInfo->TargetTranslate.y, NodeInfo->TargetTranslate.z));

		Matrix4x4 NodeTransformation = mat4Scaling * mat4Rotation * mat4Translation * ParentMatrix;
		NodeInfo->FinalTransformation = NodeTransformation;

		//根据配置骨骼名添加动态骨骼部件
		for (int i = 0; i < Impl->DyBoneInfoArray.size(); i++)
		{
			Matrix4x4& RotMat = NodeInfo->FinalTransformation;

			Quaternion Rot;
			RotMat.GetRotation(Rot);

			auto cf_boneNameParam = Impl->DyBoneInfoArray[i];
			if (cf_boneNameParam.BoneName == NodeInfo->BoneName)
			{
				std::shared_ptr<DyTransformNode> TransformNode = std::make_shared<DyTransformNode>(NodeInfo->BoneName.c_str());
				TransformNode->SetLocalPosition(NodeInfo->TargetTranslate);
				TransformNode->SetLocalRotation(Quaternion(NodeInfo->TargetRotation.x, NodeInfo->TargetRotation.y, NodeInfo->TargetRotation.z, NodeInfo->TargetRotation.w));
				TransformNode->SetWorldPosition(Vector3(RotMat[3][0], RotMat[3][1], RotMat[3][2])) ;
				TransformNode->SetWorldRotation(Rot);

				TransformNode->SetLocalToWorld(RotMat);
				TransformNode->SetWorldToLocal(RotMat.Inverse());

				auto dmc_bone = std::make_shared<DynamicBone>();
				Impl->DyBoneArray.push_back(dmc_bone);
				NodeInfo->BoneIndex = Impl->DyBoneArray.size() - 1;

				DynamicBoneInfo db_info;
				db_info.Damping = cf_boneNameParam.Damping;
				db_info.Elasticity = cf_boneNameParam.Elasticity;
				db_info.Stiffness = cf_boneNameParam.Stiffness;
				db_info.Inert = cf_boneNameParam.Inert;
				db_info.Radius = cf_boneNameParam.Radius;
				db_info.EndLength = cf_boneNameParam.EndLength;
				db_info.EndOffset = cf_boneNameParam.EndOffset;
				db_info.Gravity = cf_boneNameParam.Gravity;
				dmc_bone->Init(db_info);
				NodeInfo->bAttachToDynamic = true;

				auto Iter = Impl->TransfromMap.find(NodeInfo->BoneName);
				if (Iter == Impl->TransfromMap.end())
				{
					Impl->TransfromMap[NodeInfo->BoneName] = TransformNode;
				}

			}
			else if (!NodeInfo->ParentNode.expired() && NodeInfo->ParentNode.lock()->bAttachToDynamic == true)
			{
				//寻找父节点所对应的transform接口
				auto Iter = Impl->TransfromMap.find(NodeInfo->ParentNode.lock()->BoneName);
				if (Iter != Impl->TransfromMap.end())
				{
					auto WorldRotationMat = NodeInfo->FinalTransformation;
					std::shared_ptr<DyTransformNode> transformNode = std::make_shared<DyTransformNode>(NodeInfo->BoneName.c_str());

					transformNode->SetLocalPosition(NodeInfo->TargetTranslate);
					transformNode->SetLocalRotation(Quaternion(NodeInfo->TargetRotation.x, NodeInfo->TargetRotation.y, NodeInfo->TargetRotation.z, NodeInfo->TargetRotation.w));
					transformNode->SetWorldPosition(Vector3(WorldRotationMat[3][0], WorldRotationMat[3][1], WorldRotationMat[3][2]));
					transformNode->SetWorldRotation(Rot);

					transformNode->SetLocalToWorld(RotMat);
					transformNode->SetWorldToLocal(RotMat.Inverse());

					Iter->second->AddChildNode(transformNode.get());
					Impl->TransfromMap[NodeInfo->BoneName] = transformNode;
					NodeInfo->bAttachToDynamic = true;
					break;
				}
			}
		}

		for (int i = 0; i < NodeInfo->ChildrenNodes.size(); i++)
		{
			DFSInitDynamicBoneNode(NodeInfo->ChildrenNodes[i], NodeTransformation);
		}
	}

	void DynamicBoneManager::DynamicBonePreUpdate(std::shared_ptr<GltfBoneNodeInfo> NodeInfo, const math::Matrix4x4& ParentMatrix)
	{
		Matrix4x4 mat4Scaling = Matrix4x4::ScaleMatrix(Vector3(NodeInfo->TargetScale.x, NodeInfo->TargetScale.y, NodeInfo->TargetScale.z));
		Matrix4x4 mat4Rotation = Matrix4x4::CreateFromQuaternion(Quaternion(NodeInfo->TargetRotation));
		Matrix4x4 mat4Translation = Matrix4x4::CreateFromTranslate(Vector3(NodeInfo->TargetTranslate.x, NodeInfo->TargetTranslate.y, NodeInfo->TargetTranslate.z));

		Matrix4x4 NodeTransformation = mat4Scaling * mat4Rotation * mat4Translation * ParentMatrix;
		NodeInfo->FinalTransformation = NodeTransformation;

		auto Iter = Impl->TransfromMap.find(NodeInfo->BoneName);
		if (Iter != Impl->TransfromMap.end())
		{
			auto& RotMat = NodeInfo->FinalTransformation;
			auto& TransformNode = Iter->second;
			Quaternion Rot;
			RotMat.GetRotation(Rot);

			TransformNode->SetLocalPosition(NodeInfo->TargetTranslate);
			TransformNode->SetLocalRotation(Quaternion(NodeInfo->TargetRotation.x, NodeInfo->TargetRotation.y, NodeInfo->TargetRotation.z, NodeInfo->TargetRotation.w));
			TransformNode->SetWorldPosition(Vector3(RotMat[3][0], RotMat[3][1], RotMat[3][2]));
			TransformNode->SetWorldRotation(Rot);

			TransformNode->SetLocalToWorld(RotMat);
			TransformNode->SetWorldToLocal(RotMat.Inverse());

		}

		for (int i = 0; i < NodeInfo->ChildrenNodes.size(); i++)
		{
			DynamicBonePreUpdate(NodeInfo->ChildrenNodes[i], NodeTransformation);
		}
	}

	void DynamicBoneManager::LateUpdate(int BoneIndex /*= -1*/, float DeltaTime)
	{
		if (BoneIndex == -1 || BoneIndex >= Impl->DyBoneArray.size())
		{
			return;
		}

		Impl->DyBoneArray[BoneIndex]->Update(DeltaTime);
	}

	void DynamicBoneManager::ResetDynamicBone()
	{
		Impl->TransfromMap.clear();
		Impl->DyBoneArray.clear();
		Impl->DyBoneInfoArray.clear();
	}

	void DynamicBoneManager::DeleteDynamicBone(const std::string& BoneName)
	{
		{
			auto Iter = Impl->TransfromMap.find(BoneName);
			if (Iter != Impl->TransfromMap.end())
			{
				auto ChildNode = Iter->second;
				std::string ChildName;
				if (ChildNode->GetFirstChild())
				{
					ChildName = ChildNode->GetFirstChild()->GetID();
				}
				Iter = Impl->TransfromMap.erase(Iter);

				if (!ChildName.empty())
				{
					DeleteChildTransfromNode(ChildName);
				}
			}
		}


		for (auto Iter = Impl->DyBoneArray.begin(); Iter != Impl->DyBoneArray.end(); )
		{
			auto Bone = *Iter;
			if (Bone->GetID() == BoneName)
			{
				Iter = Impl->DyBoneArray.erase(Iter);

			}
			else
			{
				++Iter;
			}
		}

		for (auto Iter = Impl->DyBoneInfoArray.begin(); Iter != Impl->DyBoneInfoArray.end(); )
		{
			if ((*Iter).BoneName == BoneName)
			{
				Iter = Impl->DyBoneInfoArray.erase(Iter);
			}
			else
			{
				++Iter;
			}
		}
	}

	void DynamicBoneManager::DeleteChildTransfromNode(const std::string& BoneName)
	{
		auto Iter = Impl->TransfromMap.find(BoneName);
		if (Iter != Impl->TransfromMap.end())
		{
			auto ChildNode = Iter->second;
			std::string ChildName;
			if (ChildNode->GetFirstChild())
			{
				ChildName = ChildNode->GetFirstChild()->GetID();
			}
			Iter = Impl->TransfromMap.erase(Iter);

			if (!ChildName.empty())
			{
				DeleteChildTransfromNode(ChildName);
			}
		}
	}

	void DynamicBoneManager::UpdateDynamicBoneParameter(const DynamicBoneInfo& param)
	{
		for (auto item : Impl->DyBoneArray)
		{
			if (item->GetID() == param.BoneName)
			{
				DynamicBoneInfo db_info;
				db_info.Damping = param.Damping;
				db_info.Elasticity = param.Elasticity;
				db_info.Stiffness = param.Stiffness;
				db_info.Inert = param.Inert;
				db_info.Radius = param.Radius;
				db_info.EndLength = param.EndLength;
				db_info.EndOffset = param.EndOffset;
				db_info.Gravity = param.Gravity;
				item->UpdateParticleParam(db_info);
			}
		}
	}

	void DynamicBoneManager::AddBoneParam(const DynamicBoneInfo& BoneInfo)
	{
		Impl->DyBoneInfoArray.push_back(BoneInfo);
	}

}