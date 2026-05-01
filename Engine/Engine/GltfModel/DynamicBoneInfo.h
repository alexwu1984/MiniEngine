#pragma once
#include "math/vector3.h"
#include "math/vector4.h"
#include "math/matrix4x4.h"

namespace Engine
{
	struct GltfBoneNodeInfo : public std::enable_shared_from_this< GltfBoneNodeInfo>
	{
		GltfBoneNodeInfo()
		{
			Rotation = math::Vector4(0, 0, 0, 1);
			Scale = math::Vector3(1, 1, 1);
			Translate = math::Vector3(0, 0, 0);
			WorldPosition = math::Vector3(0,0,0);
			WorldRotation = math::Vector4(0, 0, 0, 1);
		}
		~GltfBoneNodeInfo()
		{
		}
		bool IsChildOfMaskBone(const std::string& BoneName)
		{
			if (BoneName.empty())
			{
				return true;
			}
			bool bFind = false;
			auto Node = this->shared_from_this();
			if (Node->BoneName == BoneName)
			{
				bFind = true;
				return bFind;
			}
			
			while (!Node->ParentNode.expired())
			{
				Node = Node->ParentNode.lock();
				if (Node->BoneName == BoneName)
				{
					bFind = true;
					break;
				}
			}

			return bFind;
		}
		std::string BoneName;
		math::Matrix4x4 InverseBindMat;
		math::Matrix4x4 FinalTransformation;
		math::Matrix4x4 InitMat;
		math::Matrix4x4 ParentMat;
		math::Matrix4x4 TPosMat;

		math::Vector4 Rotation;
		math::Vector3 Scale;
		math::Vector3 Translate;

		math::Vector4 TargetRotation;
		math::Vector3 TargetScale;
		math::Vector3 TargetTranslate;

		math::Vector4 WorldRotation;
		math::Vector3 WorldPosition;

		int NodeID = -1;
		int BoneIndex = -1;
		bool bAttachToDynamic = false;
		std::weak_ptr<GltfBoneNodeInfo> ParentNode;
		std::vector<std::shared_ptr<GltfBoneNodeInfo>> ChildrenNodes;
	};

	struct FDynamicBoneInfo
	{
		std::string BoneName;
		float Damping{ 0.06f };			
		float Elasticity{ 0.01f };		
		float Stiffness{ 0.01f };		
		float Inert{ 0.14f };			
		float Radius{ 0.0f };			
		float EndLength{ 0.0f };
		float UpdateScale{ 1.0f }; //for Force and Elasticity
		math::Vector3 EndOffset;
		math::Vector3 Gravity ;
		math::Vector3 Force ;
	};

	struct BoneSkinInfo
	{
		std::shared_ptr<GltfBoneNodeInfo> Node ;
		math::Matrix4x4 InverseBindMat;
		math::Matrix4x4 FinalMat;
	};
}
