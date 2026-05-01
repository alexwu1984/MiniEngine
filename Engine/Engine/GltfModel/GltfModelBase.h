#pragma once
#include "tinygltf/tiny_gltf.h"
#include "core/inc.h"

namespace Engine
{
	class GltfModelBase
	{
	public:
		GltfModelBase(tinygltf::Model* gltfModel);

		void* Getdata(int32_t attributeIndex, uint32_t& nCount, int32_t& CommpontType);

	protected:
		tinygltf::Model* _GltfModel = nullptr;
	private:
		std::vector<std::shared_ptr<uint8_t>> _DataBuffer;
	};
}