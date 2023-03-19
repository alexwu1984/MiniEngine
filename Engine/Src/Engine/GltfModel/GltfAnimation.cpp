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

	}

	float GltfAnimation::GetAnimationTime() const
	{
		return _AnimateAllTime;
	}

}