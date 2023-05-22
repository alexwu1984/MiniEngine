#include "GltfModel/DynamicBone.h"

namespace Engine
{
	struct DynamicBoneP
	{
		// 由这个去驱动 一般就是跟着姿态矩阵走
		DyTransformNode* GPRoot = nullptr;
		math::Vector3 Gravity;
		math::Vector3 LocalGravity;
		math::Vector3 ObjectMove;
		math::Vector3 ObjectPrevPosition;
		math::Vector3 EndOffset;

		float BoneTotalLength{ 0 };
		float ObjectScale{ 0.f };
		float Time{ 0.f };
		float Weight{ 0.f };
		float EndLength{ 0.f };
		// 我们默认30帧
		float UpdateRate{ 30.f };
	};

	DynamicBone::DynamicBone()
		:Impl(new DynamicBoneP())
	{

	}

	DynamicBone::~DynamicBone()
	{
		delete Impl;
	}

}