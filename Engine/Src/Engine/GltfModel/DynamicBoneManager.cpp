#include "GltfModel/DynamicBoneManager.h"
#include "GltfModel/DynamicBone.h"
#include "GltfModel/DyTransfromNode.h"

using namespace math;

namespace Engine
{
	struct DynamicBoneManagerPrivate
	{
		std::unordered_map<std::string, std::shared_ptr<FDyTransformNode>>	TransfromMap;
		std::vector<FDynamicBoneInfo>	DyBoneInfoArray;
		std::vector<std::shared_ptr<FDynamicBone>>	DyBoneArray;
	};

	FDynamicBoneManager::FDynamicBoneManager()
		:Impl(new DynamicBoneManagerPrivate())
	{

	}

	FDynamicBoneManager::~FDynamicBoneManager()
	{

		delete Impl;
	}

	void FDynamicBoneManager::InitParticle(std::shared_ptr<FDyTransformNode> TransNode, int32_t BoneIndex /*= -1*/)
	{
		if (TransNode == nullptr || BoneIndex == -1 || BoneIndex >= Impl->DyBoneArray.size())
		{
			return;
		}

		Impl->DyBoneArray[BoneIndex]->InitParticle(TransNode.get());
	}

	void FDynamicBoneManager::InitTransfrom(int32_t BoneIndex /*= -1*/)
	{
		if (BoneIndex == -1 || BoneIndex >= Impl->DyBoneArray.size())
		{
			return;
		}

		Impl->DyBoneArray[BoneIndex]->InitTransform();
	}

	std::shared_ptr<Engine::FDyTransformNode> FDynamicBoneManager::GetTransformNode(const std::string& BoneName)
	{
		auto Iter = Impl->TransfromMap.find(BoneName);
		if (Iter != Impl->TransfromMap.end())
		{
			return Iter->second;
		}
		return nullptr;
	}

	void FDynamicBoneManager::DFSInitDynamicBoneNode(std::shared_ptr<GltfBoneNodeInfo> NodeInfo, const math::Matrix4x4& ParentMatrix)
	{
		Matrix4x4 mat4Scaling = Matrix4x4::ScaleMatrix(NodeInfo->TargetScale);
		Matrix4x4 mat4Rotation = Matrix4x4::CreateFromQuaternion(Quaternion(NodeInfo->TargetRotation));
		Matrix4x4 mat4Translation = Matrix4x4::CreateFromTranslate(NodeInfo->TargetTranslate);

		Matrix4x4 NodeTransformation = mat4Scaling * mat4Rotation * mat4Translation * ParentMatrix;
		NodeInfo->FinalTransformation = NodeTransformation;

		for (int i = 0; i < Impl->DyBoneInfoArray.size(); i++)
		{
			Matrix4x4& RotMat = NodeInfo->FinalTransformation;

			Quaternion Rot;
			RotMat.GetRotation(Rot);

			const auto& BoneInfoParam = Impl->DyBoneInfoArray[i];
			if (BoneInfoParam.BoneName == NodeInfo->BoneName)
			{
				std::shared_ptr<FDyTransformNode> TransformNode = std::make_shared<FDyTransformNode>(NodeInfo->BoneName.c_str());
				TransformNode->SetLocalPosition(NodeInfo->TargetTranslate);
				TransformNode->SetLocalRotation(Quaternion(NodeInfo->TargetRotation.x, NodeInfo->TargetRotation.y, NodeInfo->TargetRotation.z, NodeInfo->TargetRotation.w));
				TransformNode->SetWorldPosition(Vector3(RotMat[3][0], RotMat[3][1], RotMat[3][2])) ;
				TransformNode->SetWorldRotation(Rot);

				TransformNode->SetLocalToWorld(RotMat);
				TransformNode->SetWorldToLocal(RotMat.Inverse());

				auto DyBone = std::make_shared<FDynamicBone>();
				Impl->DyBoneArray.push_back(DyBone);
				NodeInfo->BoneIndex = Impl->DyBoneArray.size() - 1;

				FDynamicBoneInfo BoneInfo;
				BoneInfo.Damping = BoneInfoParam.Damping;
				BoneInfo.Elasticity = BoneInfoParam.Elasticity;
				BoneInfo.Stiffness = BoneInfoParam.Stiffness;
				BoneInfo.Inert = BoneInfoParam.Inert;
				BoneInfo.Radius = BoneInfoParam.Radius;
				BoneInfo.EndLength = BoneInfoParam.EndLength;
				BoneInfo.EndOffset = BoneInfoParam.EndOffset;
				BoneInfo.Gravity = BoneInfoParam.Gravity;
				BoneInfo.UpdateScale = BoneInfoParam.UpdateScale;
				DyBone->Init(BoneInfo);
				NodeInfo->bAttachToDynamic = true;

				auto Iter = Impl->TransfromMap.find(NodeInfo->BoneName);
				if (Iter == Impl->TransfromMap.end())
				{
					Impl->TransfromMap[NodeInfo->BoneName] = TransformNode;
				}

			}
			else if (!NodeInfo->ParentNode.expired() && NodeInfo->ParentNode.lock()->bAttachToDynamic == true)
			{
				auto Iter = Impl->TransfromMap.find(NodeInfo->ParentNode.lock()->BoneName);
				if (Iter != Impl->TransfromMap.end())
				{
					auto WorldRotationMat = NodeInfo->FinalTransformation;
					std::shared_ptr<FDyTransformNode> transformNode = std::make_shared<FDyTransformNode>(NodeInfo->BoneName.c_str());

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

	void FDynamicBoneManager::DynamicBonePreUpdate(std::shared_ptr<GltfBoneNodeInfo> NodeInfo, const math::Matrix4x4& ParentMatrix)
	{
		Matrix4x4 mat4Scaling = Matrix4x4::ScaleMatrix(NodeInfo->TargetScale);
		Matrix4x4 mat4Rotation = Matrix4x4::CreateFromQuaternion(Quaternion(NodeInfo->TargetRotation));
		Matrix4x4 mat4Translation = Matrix4x4::CreateFromTranslate(NodeInfo->TargetTranslate);

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

		for (int32_t index = 0; index < NodeInfo->ChildrenNodes.size(); index++)
		{
			DynamicBonePreUpdate(NodeInfo->ChildrenNodes[index], NodeTransformation);
		}
	}

	void FDynamicBoneManager::LateUpdate(int BoneIndex)
	{
		if (BoneIndex == -1 || BoneIndex >= Impl->DyBoneArray.size())
		{
			return;
		}

		Impl->DyBoneArray[BoneIndex]->Update();
	}

	void FDynamicBoneManager::ResetDynamicBone()
	{
		Impl->TransfromMap.clear();
		Impl->DyBoneArray.clear();
		Impl->DyBoneInfoArray.clear();
	}

	void FDynamicBoneManager::DeleteDynamicBone(const std::string& BoneName)
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

	void FDynamicBoneManager::DeleteChildTransfromNode(const std::string& BoneName)
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

	void FDynamicBoneManager::UpdateDynamicBoneParameter(const FDynamicBoneInfo& param)
	{
		for (auto item : Impl->DyBoneArray)
		{
			if (item->GetID() == param.BoneName)
			{
				FDynamicBoneInfo boneInfo;
				boneInfo.Damping = param.Damping;
				boneInfo.Elasticity = param.Elasticity;
				boneInfo.Stiffness = param.Stiffness;
				boneInfo.Inert = param.Inert;
				boneInfo.Radius = param.Radius;
				boneInfo.EndLength = param.EndLength;
				boneInfo.EndOffset = param.EndOffset;
				boneInfo.Gravity = param.Gravity;
				boneInfo.UpdateScale = param.UpdateScale;
				item->UpdateParticleParam(boneInfo);
			}
		}
	}

	void FDynamicBoneManager::AddBoneParam(const FDynamicBoneInfo& BoneInfo)
	{
		Impl->DyBoneInfoArray.push_back(BoneInfo);
	}

}