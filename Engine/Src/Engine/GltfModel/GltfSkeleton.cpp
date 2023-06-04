#include "GltfModel/GltfSkeleton.h"
#include "math/quaternion.h"
#include "math/matrix4x4.h"
#include "GltfModel/DynamicBoneManager.h"

namespace Engine
{
	struct GltfSkeletonPrivate
	{
		std::vector<std::shared_ptr<GltfBoneNodeInfo>>	_BoneNode;
		std::vector<std::vector<BoneSkinInfo>>			_BoneNodeArray;

		//骨骼名对应的骨骼ID
		std::map<std::string, int32_t>					_BoneMap;
		//NodeID对应的骨骼ID
		std::map<int32_t, int32_t>						_NodeBoneMap;
		std::vector<std::shared_ptr<GltfBoneNodeInfo>>	_RootNode;

		std::shared_ptr<GltfNode>						_ModelNode;
		int32_t											_BoneIndex = 0;
		std::shared_ptr< DynamicBoneManager>            _DynamicBoneMgr;
	};

	void MatToTRS(const float (&m)[16], std::shared_ptr<GltfBoneNodeInfo> NodeInfo)
	{
		memcpy(NodeInfo->InitMat.m, m, sizeof(float) * 16);
		NodeInfo->Translate = math::Vector3(NodeInfo->InitMat[3][0], NodeInfo->InitMat[3][1], NodeInfo->InitMat[3][2]);
		NodeInfo->InitMat[3][0] = 0.0; NodeInfo->InitMat[3][1] = 0.0; NodeInfo->InitMat[3][2] = 0.0; NodeInfo->InitMat[3][3] = 1.0;
		//math::Vector3 tmp = math::Vector3(NodeInfo->InitMat[0][0], NodeInfo->InitMat[0][1], NodeInfo->InitMat[0][2]);
		//float s = tmp.GetLength();
		//NodeInfo->Scale = math::Vector3(s, s, s);
		//for (int i = 0; i < 15; i++)
		//{
		//	m[i] /= s;
		//}
	}

	GltfSkeleton::GltfSkeleton(tinygltf::Model* gltfModel, std::shared_ptr<GltfNode> Node)
		:GltfModelBase(gltfModel)
		, Impl(new GltfSkeletonPrivate())
	{
		Impl->_ModelNode = Node;
		Impl->_DynamicBoneMgr = std::make_shared<DynamicBoneManager>();
	}

	GltfSkeleton::~GltfSkeleton()
	{
		delete Impl;
	}

	void GltfSkeleton::InitSkeleton()
	{
		auto& Skins = _GltfModel->skins;
		auto& Nodes = _GltfModel->nodes;
		Impl->_BoneNodeArray.resize(Skins.size());
		Impl->_BoneIndex = 0;

		for (int SkinIndex = 0; SkinIndex < Skins.size(); SkinIndex++)
		{
			auto& skin = Skins[SkinIndex];

			auto& joints = skin.joints;
			auto& MatID = skin.inverseBindMatrices;

			uint32_t nCount = 0;
			int type = 0;
			math::Matrix4x4* pMat = (math::Matrix4x4*)Getdata(MatID, nCount, type);
			if (nCount != joints.size())
			{
				continue;;
			}
			BoneSkinInfo SkinInfo;
			for (int i = 0; i < nCount; i++)
			{
				auto NodeID = joints[i];
				CreateModelBoneTree(NodeID);
				std::shared_ptr<GltfBoneNodeInfo> Node = Impl->_BoneNode[Impl->_NodeBoneMap[NodeID]];
				Node->NodeID = NodeID;
				Node->InverseBindMat = pMat[i];
				SkinInfo.Node = Node;
				SkinInfo.InverseBindMat = pMat[i];
				Impl->_BoneNodeArray[SkinIndex].push_back(SkinInfo);

			}
		}

		for (int i = 0; i < Impl->_BoneNode.size(); i++)
		{
			if (Impl->_BoneNode[i]->ParentNode.expired())
			{
				Impl->_RootNode.push_back(Impl->_BoneNode[i]);
			}
		}

		UseInitPos();
		UpdateBone();

		for (int i = 0; i < Impl->_BoneNode.size(); i++)
		{
			Impl->_BoneNode[i]->TPosMat = Impl->_BoneNode[i]->FinalTransformation;
		}
	}

	void GltfSkeleton::UpdateBone()
	{

	}

	void GltfSkeleton::UseInitPos()
	{
		for (int i = 0; i < Impl->_BoneNode.size(); i++)
		{
			auto NodeInfo = Impl->_BoneNode[i];
			NodeInfo->TargetRotation = NodeInfo->Rotation;
			NodeInfo->TargetScale = NodeInfo->Scale;
			NodeInfo->TargetTranslate = NodeInfo->Translate;
		}
	}


	void GltfSkeleton::AddDynamicBone(const std::vector<DynamicBoneInfo>& BoneInfoArray)
	{
		Impl->_DynamicBoneMgr->ResetDynamicBone();
		for (int i = 0; i < BoneInfoArray.size(); ++i)
		{
			Impl->_DynamicBoneMgr->AddBoneParam(BoneInfoArray[i]);
		}

		InitDynamicBoneNode();
	}

	void GltfSkeleton::ResetDynamicBone()
	{

	}

	void GltfSkeleton::DeleteDynamicBone(const std::string& db_name)
	{

	}

	void GltfSkeleton::UpdateDynamicBoneParameter(const DynamicBoneInfo& param)
	{

	}

	bool GltfSkeleton::HasDynamicBone() const
	{
		return false;
	}

	std::vector<std::vector<Engine::BoneSkinInfo>>& GltfSkeleton::GetBoneNodeArray()
	{
		return Impl->_BoneNodeArray;
	}

	void GltfSkeleton::CreateModelBoneTree(int32_t NodeID)
	{
		if (Impl->_NodeBoneMap.find(NodeID) != Impl->_NodeBoneMap.end())
		{
			return;
		}

		auto& Nodes = _GltfModel->nodes;

		if (NodeID < Nodes.size())
		{
			std::shared_ptr<GltfBoneNodeInfo> BoneNodeInfo = std::make_shared<GltfBoneNodeInfo>();
			auto& node = Nodes[NodeID];
			std::string BoneName = node.name;
			Impl->_BoneMap[BoneName] = Impl->_BoneIndex;
			Impl->_NodeBoneMap[NodeID] = Impl->_BoneIndex;
			BoneNodeInfo->BoneName = BoneName;
			BoneNodeInfo->NodeID = NodeID;
			if (node.matrix.size() == 16)
			{
				float mat16[16]{};
				for (int m = 0; m < 16; m++)
				{
					mat16[m] = node.matrix[m];
				}
				MatToTRS(mat16, BoneNodeInfo);
			}

			if (node.scale.size() == 3)
			{
				BoneNodeInfo->Scale.x = node.scale[0];
				BoneNodeInfo->Scale.y = node.scale[1];
				BoneNodeInfo->Scale.z = node.scale[2];
			}
			if (node.translation.size() == 3)
			{
				BoneNodeInfo->Translate.x = node.translation[0];
				BoneNodeInfo->Translate.y = node.translation[1];
				BoneNodeInfo->Translate.z = node.translation[2];
			}
			if (node.rotation.size() == 4)
			{
				BoneNodeInfo->Rotation.x = node.rotation[0];
				BoneNodeInfo->Rotation.y = node.rotation[1];
				BoneNodeInfo->Rotation.z = node.rotation[2];
				BoneNodeInfo->Rotation.w = node.rotation[3];
			}
			Impl->_BoneNode.push_back(BoneNodeInfo);

			Impl->_BoneIndex++;

			auto& child = _GltfModel->nodes[NodeID].children;
			for (int i = 0; i < child.size(); i++)
			{
				int ChildNodeID = child[i];
				CreateModelBoneTree(ChildNodeID);
				auto ChildNode = Impl->_BoneNode[Impl->_NodeBoneMap[ChildNodeID]];
				ChildNode->ParentNode = BoneNodeInfo;
				BoneNodeInfo->ChildrenNodes.push_back(ChildNode);
			}
		}
	}

	void GltfSkeleton::DFSBoneTree(std::shared_ptr< GltfBoneNodeInfo> BoneNodeInfo, math::Matrix4x4& ParentMatrix)
	{
		math::Matrix4x4 mat4Scaling = math::Matrix4x4::ScaleMatrix(math::Vector3(BoneNodeInfo->TargetScale.x, BoneNodeInfo->TargetScale.y, BoneNodeInfo->TargetScale.z));
		math::Matrix4x4 mat4Rotation = math::Matrix4x4::CreateFromQuaternion(math::Quaternion(BoneNodeInfo->TargetRotation));
		math::Matrix4x4 mat4Translation = math::Matrix4x4::CreateFromTranslate(math::Vector3(BoneNodeInfo->TargetTranslate.x, BoneNodeInfo->TargetTranslate.y, BoneNodeInfo->TargetTranslate.z));

		//不能去掉！解析模型的时候M16不一定是TRS，所以这里的InitMat存储的转换矩阵，不一定是单位阵。特别是头发处会出问题
		if ((BoneNodeInfo->TargetRotation - math::Vector4(0, 0, 0, 1)).GetLength() < 0.0001)
		{
			mat4Rotation = BoneNodeInfo->InitMat;
		}

		math::Matrix4x4 NodeTransformation = mat4Scaling * mat4Rotation * mat4Translation;
		//NodeTransformation = ParentMatrix * mat4Translation * mat4Rotation * mat4Scaling;

		//更新动态骨骼
		auto TransformBone = Impl->_DynamicBoneMgr->GetTransformNode(BoneNodeInfo->BoneName);
		if (TransformBone)
		{
			math::Matrix4x4& RotMat = NodeTransformation;
			Impl->_DynamicBoneMgr->LateUpdate(BoneNodeInfo->NodeIndex);
			math::Vector3 Scale;
			RotMat.GetScale(Scale);
			math::Vector3 Translate = RotMat.GetTranslation();
			math::Quaternion Rot;
			RotMat.GetRotation(Rot);

			auto TmpScaleMat = math::Matrix4x4::ScaleMatrix(Scale);
			auto TmpRotMat = math::Matrix4x4::CreateFromQuaternion(Rot);
			auto TmpTranslateMat = math::Matrix4x4::CreateFromTranslate(Translate);
			//最终只需要使用旋转参数进行所有的变化，所以需要把旋转的变换应用到该附属节点上，这里采用直接替换旋转四元数的方式才是正确的
			NodeTransformation = TmpScaleMat * TmpRotMat * TmpTranslateMat;
		}

		BoneNodeInfo->FinalTransformation = NodeTransformation;

		for (int i = 0; i < BoneNodeInfo->ChildrenNodes.size(); i++)
		{
			DFSBoneTree(BoneNodeInfo->ChildrenNodes[i], NodeTransformation);
		}
	}

	void GltfSkeleton::InitDynamicBoneNode()
	{
		if (Impl->_RootNode.size() <= 0)
		{
			return;
		}

		for (int i = 0; i < Impl->_RootNode.size(); i++)
		{
			math::Matrix4x4 BoneTransformation;
			BoneTransformation.Identity();
			Impl->_DynamicBoneMgr->DFSInitDynamicBoneNode(Impl->_RootNode[i], BoneTransformation);
		}

		for (int i = 0; i < Impl->_BoneNode.size(); i++)
		{
			auto NodeInfo = Impl->_BoneNode[i];

			if (NodeInfo->NodeIndex != -1)
			{
				auto TransformNode = Impl->_DynamicBoneMgr->GetTransformNode(NodeInfo->BoneName);
				if (TransformNode)
				{
					Impl->_DynamicBoneMgr->InitParticle(TransformNode, NodeInfo->NodeIndex);
					Impl->_DynamicBoneMgr->InitTransfrom(NodeInfo->NodeIndex);
				}
			}
		}
	}

}