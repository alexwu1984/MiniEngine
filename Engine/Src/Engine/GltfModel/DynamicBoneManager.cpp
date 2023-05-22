#include "GltfModel/DynamicBoneManager.h"
#include "GltfModel/DynamicBone.h"

namespace Engine
{
	struct DynamicBoneManagerP
	{
		//动态骨骼各个particle的transform接口
		std::unordered_map<std::string, std::shared_ptr<DyTransformNode>>		TransfromMap;
		//动态骨骼配置项参数
		std::vector<DynamicBoneInfo>					DyBoneNamesArray;
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

}