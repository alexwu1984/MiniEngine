#include "GltfModel/GltfModelBase.h"

namespace Engine
{
	GltfModelBase::GltfModelBase(tinygltf::Model* gltfModel)
		:_GltfModel(gltfModel)
	{

	}

	void* GltfModelBase::Getdata(int32_t attributeIndex, uint32_t& nCount, int32_t& CommpontType)
	{
		const auto& indicesAccessor = _GltfModel->accessors[attributeIndex];
		if (indicesAccessor.bufferView < 0)
			return nullptr;
		const auto& bufferView = _GltfModel->bufferViews[indicesAccessor.bufferView];
		const auto& buffer = _GltfModel->buffers[bufferView.buffer];
		const auto dataAddress = buffer.data.data() + bufferView.byteOffset + indicesAccessor.byteOffset;
		const auto byteStride = indicesAccessor.ByteStride(bufferView);

		nCount = uint32_t(indicesAccessor.count);
		CommpontType = indicesAccessor.componentType;


		int type = indicesAccessor.type;
		int nStep = 0;
		if (type == TINYGLTF_TYPE_SCALAR) {
			nStep = 1;
		}
		else if (type == TINYGLTF_TYPE_VEC2) {
			nStep = 2;
		}
		else if (type == TINYGLTF_TYPE_VEC3) {

			nStep = 3;
		}
		else if (type == TINYGLTF_TYPE_VEC4) {

			nStep = 4;
		}
		int OneSize = 0;

		if (CommpontType == 5122 || CommpontType == 5123) {
			OneSize = sizeof(uint16_t);
		}
		else if (CommpontType == 5124 || CommpontType == 5125) {
			OneSize = sizeof(uint32_t);
		}
		else if (CommpontType == 5126) {

			OneSize = sizeof(float);
		}
		else if (CommpontType == 5120 || CommpontType == 5121) {
			OneSize = sizeof(uint8_t);
		}

		if (nStep == 0 || OneSize == 0 || nStep * OneSize == byteStride)
		{
			return (void*)dataAddress;
		}
		else
		{
			std::shared_ptr<uint8_t> TmpData(new uint8_t[nStep * OneSize * nCount], [](uint8_t* p) {delete[]p; });

			uint8_t* pSrc = (uint8_t*)dataAddress;
			for (uint32_t i = 0; i < nCount; ++i)
			{

				memcpy(TmpData.get() + i * OneSize * nStep, pSrc + i * byteStride, OneSize * nStep);
			}
			_DataBuffer.push_back(TmpData);

			return (void*)TmpData.get();
		}
	}

}

