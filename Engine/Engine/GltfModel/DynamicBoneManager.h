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

	private:
		DynamicBoneManagerP* Impl = nullptr;
	};
}