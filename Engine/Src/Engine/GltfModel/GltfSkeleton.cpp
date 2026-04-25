#include "GltfModel/GltfSkeleton.h"
#include "math/quaternion.h"
#include "math/matrix4x4.h"
#include "GltfModel/DynamicBoneManager.h"
#include "GltfModel/GltfNode.h"

namespace Engine
{
	struct GltfSkeletonPrivate
	{
		std::vector<std::shared_ptr<GltfBoneNodeInfo>>	_BoneNode;
		std::vector<std::vector<BoneSkinInfo>>			_BoneNodeArray;

		std::map<std::string, int32_t>					_BoneMap;
		std::map<int32_t, int32_t>						_NodeBoneMap;
		std::vector<std::shared_ptr<GltfBoneNodeInfo>>	_RootNode;

		std::shared_ptr<GltfNode>						_ModelNode;
		int32_t											_BoneIndex = 0;
		std::shared_ptr< FDynamicBoneManager>            _DynamicBoneMgr;
	};

	void MatToTRS(const float (&m)[16], std::shared_ptr<GltfBoneNodeInfo> NodeInfo)
	{
		memcpy(NodeInfo->InitMat.m, m, sizeof(float) * 16);
		NodeInfo->Translate = math::Vector3(NodeInfo->InitMat[3][0], NodeInfo->InitMat[3][1], NodeInfo->InitMat[3][2]);
		NodeInfo->InitMat[3][0] = 0.0; NodeInfo->InitMat[3][1] = 0.0; NodeInfo->InitMat[3][2] = 0.0; NodeInfo->InitMat[3][3] = 1.0;
		math::Vector3 ScaleAxis(NodeInfo->InitMat[0][0], NodeInfo->InitMat[0][1], NodeInfo->InitMat[0][2]);
		float Scale = ScaleAxis.GetLength();
		if (Scale > 0.0001f)
		{
			NodeInfo->Scale = math::Vector3(Scale, Scale, Scale);
			for (int i = 0; i < 15; ++i)
			{
				NodeInfo->InitMat.m[i] /= Scale;
			}
		}
	}

	GltfSkeleton::GltfSkeleton(tinygltf::Model* gltfModel, std::shared_ptr<GltfNode> Node)
		:GltfModelBase(gltfModel)
		, d_ptr(new GltfSkeletonPrivate())
	{
		C_P(GltfSkeleton);
		d->_ModelNode = Node;
		d->_DynamicBoneMgr = std::make_shared<FDynamicBoneManager>();
	}

	GltfSkeleton::~GltfSkeleton()
	{
		delete d_ptr;
	}

	void GltfSkeleton::InitSkeleton()
	{
		C_P(GltfSkeleton);
		auto& Skins = _GltfModel->skins;
		auto& Nodes = _GltfModel->nodes;
		d->_BoneNodeArray.resize(Skins.size());
		d->_BoneIndex = 0;

		for (int SkinIndex = 0; SkinIndex < Skins.size(); SkinIndex++)
		{
			auto& skin = Skins[SkinIndex];

			auto& joints = skin.joints;
			auto& MatID = skin.inverseBindMatrices;

			uint32_t nCount = 0;
			int type = 0;
			math::Matrix4x4* Matrices = (math::Matrix4x4*)Getdata(MatID, nCount, type);
			if (nCount != joints.size())
			{
				continue;;
			}
			BoneSkinInfo SkinInfo;
			for (uint32_t i = 0; i < nCount; i++)
			{
				auto& InverseBindMat = Matrices[i];
				auto NodeID = joints[i];
				CreateModelBoneTree(NodeID);
				std::shared_ptr<GltfBoneNodeInfo> Node = d->_BoneNode[d->_NodeBoneMap[NodeID]];
				Node->InverseBindMat = InverseBindMat;
				SkinInfo.Node = Node;
				SkinInfo.InverseBindMat = InverseBindMat;
				d->_BoneNodeArray[SkinIndex].push_back(SkinInfo);

			}
		}

		for (uint32_t i = 0; i < (uint32_t)d->_BoneNode.size(); i++)
		{
			if (d->_BoneNode[i]->ParentNode.expired())
			{
				d->_RootNode.push_back(d->_BoneNode[i]);
			}
		}

		UseInitPos();
		UpdateBone();

		for (uint32_t i = 0; i < (uint32_t)d->_BoneNode.size(); i++)
		{
			d->_BoneNode[i]->TPosMat = d->_BoneNode[i]->FinalTransformation;
		}
	}

	void GltfSkeleton::UpdateBone()
	{
		C_P(GltfSkeleton);
		if (d->_RootNode.empty())
			return;

		for (uint32_t i = 0; i < (uint32_t)d->_RootNode.size(); i++)
		{
			int NodeID = d->_RootNode[i]->NodeID;
			auto ParentNode = d->_ModelNode->GetAllNodes()[NodeID]->ParentNode;

			auto Identity = d->_RootNode[i]->ParentMat;
			if (!ParentNode.expired())
			{
				d->_ModelNode->UpdateNodeParent(ParentNode.lock());
				Identity *= ParentNode.lock()->FinalMeshMat;
			}
			d->_DynamicBoneMgr->DynamicBonePreUpdate(d->_RootNode[i], Identity);
			DFSBoneTree(d->_RootNode[i], Identity);
		}


		for (uint32_t i = 0; i < (uint32_t)d->_BoneNodeArray.size(); i++)
		{
			for (uint32_t j = 0; j < (uint32_t)d->_BoneNodeArray[i].size(); j++)
			{
				d->_BoneNodeArray[i][j].FinalMat = d->_BoneNodeArray[i][j].Node->FinalTransformation * d->_BoneNodeArray[i][j].InverseBindMat;
			}
		}
	}

	void GltfSkeleton::UseInitPos()
	{
		C_P(GltfSkeleton);
		for (uint32_t i = 0; i < (uint32_t)d->_BoneNode.size(); i++)
		{
			auto NodeInfo = d->_BoneNode[i];
			NodeInfo->TargetRotation = NodeInfo->Rotation;
			NodeInfo->TargetScale = NodeInfo->Scale;
			NodeInfo->TargetTranslate = NodeInfo->Translate;
		}
	}


	void GltfSkeleton::AddDynamicBone(const std::vector<FDynamicBoneInfo>& BoneInfoArray)
	{
		C_P(GltfSkeleton);
		d->_DynamicBoneMgr->ResetDynamicBone();
		for (uint32_t i = 0; (uint32_t)i < BoneInfoArray.size(); ++i)
		{
			d->_DynamicBoneMgr->AddBoneParam(BoneInfoArray[i]);
		}

		InitDynamicBoneNode();
	}

	void GltfSkeleton::ResetDynamicBone()
	{
		C_P(GltfSkeleton);
		d->_DynamicBoneMgr->ResetDynamicBone();
	}

	void GltfSkeleton::DeleteDynamicBone(const std::string& BoneName)
	{
		C_P(GltfSkeleton);
		d->_DynamicBoneMgr->DeleteDynamicBone(BoneName);
	}

	void GltfSkeleton::UpdateDynamicBoneParameter(const FDynamicBoneInfo& param)
	{
		C_P(GltfSkeleton);
		d->_DynamicBoneMgr->UpdateDynamicBoneParameter(param);
	}

	bool GltfSkeleton::HasDynamicBone() const
	{
		return false;
	}

	std::vector<std::vector<Engine::BoneSkinInfo>>& GltfSkeleton::GetBoneNodeArray()
	{
		C_P(GltfSkeleton);
		return d->_BoneNodeArray;
	}

	std::vector<std::shared_ptr<Engine::GltfBoneNodeInfo>>& GltfSkeleton::GetRootNode()
	{
		C_P(GltfSkeleton);
		return d->_RootNode;
	}

	std::shared_ptr<GltfBoneNodeInfo> GltfSkeleton::GetBoneNodeByNodeId(int32_t NodeID) const
	{
		C_P(const GltfSkeleton);
		auto Iter = d->_NodeBoneMap.find(NodeID);
		if (Iter == d->_NodeBoneMap.end())
		{
			return nullptr;
		}

		return d->_BoneNode[Iter->second];
	}

	void GltfSkeleton::CreateModelBoneTree(int32_t NodeID)
	{
		C_P(GltfSkeleton);
		if (d->_NodeBoneMap.find(NodeID) != d->_NodeBoneMap.end())
			return;

		auto& Nodes = _GltfModel->nodes;

		if (NodeID < (int32_t)Nodes.size())
		{
			std::shared_ptr<GltfBoneNodeInfo> BoneNodeInfo = std::make_shared<GltfBoneNodeInfo>();
			auto& node = Nodes[NodeID];
			std::string BoneName = node.name;
			d->_BoneMap[BoneName] = d->_BoneIndex;
			d->_NodeBoneMap[NodeID] = d->_BoneIndex;
			BoneNodeInfo->BoneName = BoneName;
			BoneNodeInfo->NodeID = NodeID;
			if (node.matrix.size() == 16)
			{
				float mat16[16]{};
				for (int m = 0; m < 16; m++)
				{
					mat16[m] = (float)node.matrix[m];
				}
				MatToTRS(mat16, BoneNodeInfo);
			}

			if (node.scale.size() == 3)
			{
				BoneNodeInfo->Scale.x = (float)node.scale[0];
				BoneNodeInfo->Scale.y = (float)node.scale[1];
				BoneNodeInfo->Scale.z = (float)node.scale[2];
			}
			if (node.translation.size() == 3)
			{
				BoneNodeInfo->Translate.x = (float)node.translation[0];
				BoneNodeInfo->Translate.y = (float)node.translation[1];
				BoneNodeInfo->Translate.z = (float)node.translation[2];
			}
			if (node.rotation.size() == 4)
			{
				BoneNodeInfo->Rotation.x = (float)node.rotation[0];
				BoneNodeInfo->Rotation.y = (float)node.rotation[1];
				BoneNodeInfo->Rotation.z = (float)node.rotation[2];
				BoneNodeInfo->Rotation.w = (float)node.rotation[3];
			}
			d->_BoneNode.push_back(BoneNodeInfo);
			d->_BoneIndex++;

			auto& child = _GltfModel->nodes[NodeID].children;
			for (uint32_t i = 0; (uint32_t)i < child.size(); i++)
			{
				int ChildNodeID = child[i];
				CreateModelBoneTree(ChildNodeID);
				auto ChildNode = d->_BoneNode[d->_NodeBoneMap[ChildNodeID]];
				ChildNode->ParentNode = BoneNodeInfo;
				BoneNodeInfo->ChildrenNodes.push_back(ChildNode);
			}
		}
	}

	void GltfSkeleton::DFSBoneTree(std::shared_ptr< GltfBoneNodeInfo> BoneNodeInfo, math::Matrix4x4& ParentMatrix)
	{
		C_P(GltfSkeleton);
		math::Matrix4x4 mat4Scaling = math::Matrix4x4::ScaleMatrix(math::Vector3(BoneNodeInfo->TargetScale.x, BoneNodeInfo->TargetScale.y, BoneNodeInfo->TargetScale.z));
		math::Matrix4x4 mat4Rotation = math::Matrix4x4::CreateFromQuaternion(math::Quaternion(BoneNodeInfo->TargetRotation));
		math::Matrix4x4 mat4Translation = math::Matrix4x4::CreateFromTranslate(math::Vector3(BoneNodeInfo->TargetTranslate.x, BoneNodeInfo->TargetTranslate.y, BoneNodeInfo->TargetTranslate.z));

		if ((BoneNodeInfo->TargetRotation - math::Vector4(0, 0, 0, 1)).GetLength() < 0.0001)
			mat4Rotation = BoneNodeInfo->InitMat;

		math::Matrix4x4 NodeTransformation = ParentMatrix * mat4Translation * mat4Rotation * mat4Scaling;

		auto TransformBone = d->_DynamicBoneMgr->GetTransformNode(BoneNodeInfo->BoneName);
		if (TransformBone)
		{
			math::Matrix4x4& RotMat = NodeTransformation;
			d->_DynamicBoneMgr->LateUpdate(BoneNodeInfo->BoneIndex);
			math::Vector3 Scale;
			RotMat.GetScale(Scale);
			math::Vector3 Translate = TransformBone->GetWorldPosition();
			math::Quaternion Rot = TransformBone->GetWorldRotation();

			auto TmpScaleMat = math::Matrix4x4::ScaleMatrix(Scale);
			auto TmpRotMat = math::Matrix4x4::CreateFromQuaternion(Rot);
			auto TmpTranslateMat = math::Matrix4x4::CreateFromTranslate(Translate);
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
		C_P(GltfSkeleton);
		if (d->_RootNode.size() <= 0)
			return;

		for (uint32_t i = 0; i < (uint32_t)d->_RootNode.size(); i++)
		{
			math::Matrix4x4 BoneTransformation;
			BoneTransformation.Identity();
			d->_DynamicBoneMgr->DFSInitDynamicBoneNode(d->_RootNode[i], BoneTransformation);
		}

		for (uint32_t i = 0; i < (uint32_t)d->_BoneNode.size(); i++)
		{
			auto NodeInfo = d->_BoneNode[i];

			if (NodeInfo->BoneIndex != -1)
			{
				auto TransformNode = d->_DynamicBoneMgr->GetTransformNode(NodeInfo->BoneName);
				if (TransformNode)
				{
					d->_DynamicBoneMgr->InitParticle(TransformNode, NodeInfo->BoneIndex);
					d->_DynamicBoneMgr->InitTransfrom(NodeInfo->BoneIndex);
				}
			}
		}
	}

}