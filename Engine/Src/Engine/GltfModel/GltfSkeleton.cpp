#include "GltfModel/GltfSkeleton.h"
#include "math/quaternion.h"

namespace Engine
{

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
		,_ModelNode(Node)
	{

	}

	GltfSkeleton::~GltfSkeleton()
	{

	}

	void GltfSkeleton::InitSkeleton()
	{
		auto& Skins = _GltfModel->skins;
		auto& Nodes = _GltfModel->nodes;
		_BoneNodeArray.resize(Skins.size());
		_BoneIndex = 0;

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
			BoneSkinInfo pSkinInfo;
			for (int i = 0; i < nCount; i++)
			{
				auto NodeID = joints[i];
				CreateModelBoneTree(NodeID);
				std::shared_ptr<GltfBoneNodeInfo> Node = _BoneNode[_NodeBoneMap[NodeID]];
				Node->NodeID = NodeID;
				Node->InverseBindMat = pMat[i];
				pSkinInfo.Node = Node;
				pSkinInfo.InverseBindMat = pMat[i];
				_BoneNodeArray[SkinIndex].push_back(pSkinInfo);

			}
		}

		for (int i = 0; i < _BoneNode.size(); i++)
		{
			if (_BoneNode[i]->ParentNode.expired())
			{
				_RootNode.push_back(_BoneNode[i]);
			}
		}

		UseInitPos();
		UpdateBone();

		for (int i = 0; i < _BoneNode.size(); i++)
		{
			_BoneNode[i]->TPosMat = _BoneNode[i]->FinalTransformation;
		}
	}

	void GltfSkeleton::UpdateBone()
	{

	}

	void GltfSkeleton::UseInitPos()
	{
		for (int i = 0; i < _BoneNode.size(); i++)
		{
			auto NodeInfo = _BoneNode[i];
			NodeInfo->TargetRotation = NodeInfo->Rotation;
			NodeInfo->TargetScale = NodeInfo->Scale;
			NodeInfo->TargetTranslate = NodeInfo->Translate;
		}
	}

	void GltfSkeleton::UpdateNeckBone(std::vector<float>& headRotation, const char* neckBoneName)
	{

	}

	void GltfSkeleton::AddDynamicBone(const std::vector<DynamicBoneParameter>& db_paramter_array)
	{

	}

	void GltfSkeleton::ResetDynamicBone()
	{

	}

	void GltfSkeleton::DeleteDynamicBone(const std::string& db_name)
	{

	}

	void GltfSkeleton::UpdateDynamicBoneParameter(const DynamicBoneParameter& param)
	{

	}

	bool GltfSkeleton::HasDynamicBone() const
	{
		return false;
	}

	void GltfSkeleton::CreateModelBoneTree(int32_t NodeID)
	{
		if (_NodeBoneMap.find(NodeID) != _NodeBoneMap.end())
		{
			return;
		}

		auto& Nodes = _GltfModel->nodes;

		if (NodeID < Nodes.size())
		{
			std::shared_ptr<GltfBoneNodeInfo> BoneNodeInfo = std::make_shared<GltfBoneNodeInfo>();
			auto& node = Nodes[NodeID];
			std::string BoneName = node.name;
			_BoneMap[BoneName] = _BoneIndex;
			_NodeBoneMap[NodeID] = _BoneIndex;
			BoneNodeInfo->bone_name = BoneName;
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
			_BoneNode.push_back(BoneNodeInfo);

			_BoneIndex++;

			auto& child = _GltfModel->nodes[NodeID].children;
			for (int i = 0; i < child.size(); i++)
			{
				int ChildNodeID = child[i];
				CreateModelBoneTree(ChildNodeID);
				auto ChildNode = _BoneNode[_NodeBoneMap[ChildNodeID]];
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

		////更新动态骨骼
		//auto itr = dynamicBonaMgr->transformMap.find(info->bone_name);
		//if (itr != dynamicBonaMgr->transformMap.end())
		//{
		//	CC3DMath::CC3DMatrix rot_mat(NodeTransformation[0][0], NodeTransformation[1][0], NodeTransformation[2][0], NodeTransformation[3][0],
		//		NodeTransformation[0][1], NodeTransformation[1][1], NodeTransformation[2][1], NodeTransformation[3][1],
		//		NodeTransformation[0][2], NodeTransformation[1][2], NodeTransformation[2][2], NodeTransformation[3][2],
		//		NodeTransformation[0][3], NodeTransformation[1][3], NodeTransformation[2][3], NodeTransformation[3][3]);

		//	dynamicBonaMgr->LateUpdate(info->db_index);

		//	float3 de_scale, de_translate;
		//	quaternion4f de_rotate;
		//	glm::mat4 newMat;
		//	rot_mat.decompose(&de_scale, &de_rotate, &de_translate);
		//	de_translate.set(NodeTransformation[3][0], NodeTransformation[3][1], NodeTransformation[3][2]);
		//	glm::mat4 temp_scale = glm::scale(newMat, glm::vec3(de_scale.x, de_scale.y, de_scale.z));
		//	glm::mat4 temp_rotation = CC3DUtils::QuaternionToMatrix(Vector4(itr->second->worldRotation.x, itr->second->worldRotation.y, itr->second->worldRotation.z, itr->second->worldRotation.w));
		//	glm::mat4 temp_translate = glm::translate(newMat, glm::vec3(de_translate.x, de_translate.y, de_translate.z));

		//	//最终只需要使用旋转参数进行所有的变化，所以需要把旋转的变换应用到该附属节点上，这里采用直接替换旋转四元数的方式才是正确的
		//	NodeTransformation = temp_translate * temp_rotation * temp_scale;
		//}

		BoneNodeInfo->FinalTransformation = NodeTransformation;

		for (int i = 0; i < BoneNodeInfo->ChildrenNodes.size(); i++)
		{
			DFSBoneTree(BoneNodeInfo->ChildrenNodes[i], NodeTransformation);
		}
	}

}