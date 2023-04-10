#pragma once
#include "GltfModel/GltfAnimaionInfo.h"
#include "tinygltf/tiny_gltf.h"

namespace Engine
{
	class GltfModel;
	class GltfAnimation
	{
	public:
		GltfAnimation(tinygltf::Model* gltfModel, GltfModel* Model);
		~GltfAnimation();

		void InitAnimate(uint32_t AnimateIndex);
		float GetAnimationTime() const;
		bool HasModelAnimatie() const;
		void Play(float fSecond, GltfModel* Model);
	private:
		void* Getdata(int32_t attributeIndex, uint32_t& nCount, int32_t& CommpontType);
		void PlaySkeleton(float fSecond, std::shared_ptr< AnimationChannelInfo> ChannelInfo);
	private:
		tinygltf::Model* _gltfModel = nullptr;
		GltfModel* _Model = nullptr;
		std::string _AnimateName;
		float	_AnimateAllTime = 0;
		bool	_hasModelAnimate = false;
		float	_StartTime = 0.0f;
		float	_EndTime = 0.0f;
		std::vector< std::shared_ptr< AnimationChannelInfo>> _ChannelInfo;
		std::vector<std::any> DataBuffer;
	};

}
