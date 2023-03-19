#include "GltfModel/GltfAnimationManager.h"
#include "GltfModel/GltfAnimation.h"

namespace Engine 
{

	GltfAnimationManager::GltfAnimationManager(tinygltf::Model* Model)
		:_Model(Model)
	{

	}

	GltfAnimationManager::~GltfAnimationManager()
	{

	}

	void GltfAnimationManager::InitAnimation()
	{
		for (size_t index = 0; index < _Model->animations.size(); ++index)
		{
			std::shared_ptr<GltfAnimation> Animation = std::make_shared<GltfAnimation>(_Model);
			Animation->InitAnimate(index);
			_Animations.push_back(Animation);

			_AnimationAllTime = (std::max)(_AnimationAllTime, Animation->GetAnimationTime());
		}

	}

	void GltfAnimationManager::Play(float Second)
	{

	}

}
