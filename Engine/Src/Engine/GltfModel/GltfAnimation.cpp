#include "GLTFModel/GltfAnimation.h"

namespace Engine
{

	GltfAnimation::GltfAnimation(tinygltf::Model* Model)
		:_Model(Model)
	{

	}

	GltfAnimation::~GltfAnimation()
	{

	}

	void GltfAnimation::InitAnimate(uint32_t AnimateIndex)
	{
		if (AnimateIndex >= _Model->animations.size())
		{
			return;
		}
		auto& Animate = _Model->animations[AnimateIndex];

		_AnimateName = Animate.name;

		for (int i = 0; i < Animate.channels.size(); i++)
		{
			auto& pChannel = Animate.channels[i];
			std::shared_ptr<AnimationChannelInfo> pInfo = std::make_shared<AnimationChannelInfo>();
			pInfo->nNodeID = pChannel.target_node;
			std::string sType = pChannel.target_path;
			if (sType == "translation")
			{
				pInfo->eType = TARGET_TRANSLATE;
			}
			else if (sType == "rotation")
			{
				pInfo->eType = TARGET_ROTATE;
			}
			else if (sType == "scale")
			{
				pInfo->eType = TARGET_SCALE;
			}
			else if (sType == "weights")
			{
				pInfo->eType = TARGET_WEIGHT;
			}
			auto sampleID = pChannel.sampler;
			auto Sample = Animate.samplers[sampleID];

			auto input = Sample.input;
			auto output = Sample.output;
			int type = 0;
			pInfo->pInputTime = (float*)Getdata(input, pInfo->nKeyFrame, type);
			pInfo->pOutputAnimateValue = (float*)Getdata(output, pInfo->nOutCount, type);
			auto maxVaue = _Model->accessors[input].maxValues;
			_ChannelInfo.push_back(pInfo);
			if (maxVaue.size() == 1)
			{
				_AnimateAllTime = (std::max)(_AnimateAllTime * 1.0, maxVaue[0]);
			}
			if (pInfo->nKeyFrame > 0)
			{
				_AnimateAllTime = (std::max)(_AnimateAllTime, pInfo->pInputTime[pInfo->nKeyFrame - 1]);
			}

		}
	}

	float GltfAnimation::GetAnimationTime() const
	{
		return _AnimateAllTime;
	}

	void* GltfAnimation::Getdata(int32_t attributeIndex, uint32_t& nCount, int32_t& CommpontType)
	{
		const auto& indicesAccessor = _Model->accessors[attributeIndex];
		const auto& bufferView = _Model->bufferViews[indicesAccessor.bufferView];
		const auto& buffer = _Model->buffers[bufferView.buffer];
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
			DataBuffer.push_back(TmpData);

			return (void*)TmpData.get();
		}
	}

}