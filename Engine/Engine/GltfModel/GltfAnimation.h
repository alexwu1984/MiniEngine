#pragma once
#include "GltfModel/GltfAnimaionInfo.h"
#include "GltfModel/GltfModelBase.h"

namespace Engine
{
	class GltfModel;
	class GltfAnimation : public GltfModelBase
	{
	public:
		GltfAnimation(tinygltf::Model* gltfModel, GltfModel* Model);
		~GltfAnimation();

		void InitAnimate(uint32_t AnimateIndex);
		float GetAnimationTime() const;
		bool HasModelAnimatie() const;
		void Play(float fSecond, GltfModel* Model);
	private:
		
		void PlaySkeleton(float fSecond, std::shared_ptr< AnimationChannelInfo> ChannelInfo);
	private:
		
		GltfModel* _Model = nullptr;
		std::string _AnimateName;
		float	_AnimateAllTime = 0;
		bool	_hasModelAnimate = false;
		float	_StartTime = 0.0f;
		float	_EndTime = 0.0f;
		std::vector< std::shared_ptr< AnimationChannelInfo>> _ChannelInfo;
		
	};

}
