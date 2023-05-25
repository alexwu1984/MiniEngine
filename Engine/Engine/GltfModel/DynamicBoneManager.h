#pragma once
#include "GltfModel/GltfSkeleton.h"
#include "GltfModel/DyTransfromNode.h"

namespace Engine
{
	struct DynamicBoneManagerP;

	class DynamicBoneManager
	{
	public:
		DynamicBoneManager();
		~DynamicBoneManager();

		/*
  @初始化动态骨骼所持有的transfrom接口，记录位置信息与转换矩阵
  @param: db_index  动态骨骼索引，为-1时表示没有动态骨骼
*/
		//std::shared_ptr<DyTransformNode>
		void InitParticle(std::shared_ptr<DyTransformNode> TransNode, int32_t db_index = -1);
		/*
		 @动态骨骼transfrom局部坐标初始化
		 @param: db_index  动态骨骼索引，为-1时表示没有动态骨骼
		*/
		void InitTransfrom(int32_t db_index = -1);

	private:
		DynamicBoneManagerP* Impl = nullptr;
	};
}