#pragma once
#include "math/vector3.h"
#include "math/vector4.h"
#include "math/matrix4x4.h"

namespace Engine
{
	//一个Model的骨骼树节点信息
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
		bool IsChildOfMaskBone(const std::string& NodeName)
		{
			//如果mask 的骨骼名称为空，则视为使用普通blend,即所有的骨骼动画都生效
			//只有在mask的骨骼名称不为空，且当前骨骼名称不为mask骨骼的子节点，才返回false
			if (NodeName.empty())
			{
				return true;
			}
			bool bFind = false;
			auto n = this->shared_from_this();
			if (n->BoneName == NodeName)
			{
				bFind = true;
				return bFind;
			}
			
			while (!n->ParentNode.expired())
			{
				n = n->ParentNode.lock();
				if (n->BoneName == NodeName)
				{
					bFind = true;
					break;
				}
			}

			return bFind;
		}
		//节点名
		std::string BoneName;
		//骨骼原有的变化
		math::Matrix4x4 InverseBindMat;
		//动画的每一帧骨骼的最终变化
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
		//动态骨骼索引，默认为-1，表示没有动态骨骼
		int NodeIndex = -1;
		//当设置某一个节点为动态骨骼附属节点时，此节点的所有子节点都会变成动态变化的节点
		bool bAttachToDynamic = false;

		std::weak_ptr<GltfBoneNodeInfo> ParentNode;

		//子节点
		std::vector<std::shared_ptr<GltfBoneNodeInfo>> ChildrenNodes;

	};

	struct DynamicBoneInfo
	{
		std::string BoneName;
		float Damping{ 0.06f };			//阻尼
		float Elasticity{ 0.01f };		//弹性
		float Stiffness{ 0.01f };		//刚性
		float Inert{ 0.14f };			//惯性
		float Radius{ 0.0f };			//半径
		float EndLength{ 0.0f };		//
		math::Vector3 EndOffset;
		//重力
		math::Vector3 Gravity ;
		math::Vector3 Force ;

		float RadiusScale = 1.0f;
	};

	struct BoneSkinInfo
	{
		std::shared_ptr<GltfBoneNodeInfo> Node ;
		math::Matrix4x4 InverseBindMat;
		math::Matrix4x4 FinalMat;
	};
}
