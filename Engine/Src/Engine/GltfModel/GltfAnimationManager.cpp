#include "GltfModel/GltfAnimationManager.h"
#include "GltfModel/GltfAnimation.h"

namespace Engine 
{

	GltfAnimationManager::GltfAnimationManager(tinygltf::Model* gltfModel, GltfModel* Model)
		:_gltfModel(gltfModel)
		,_Model(Model)
	{

	}

	GltfAnimationManager::~GltfAnimationManager()
	{

	}

	void GltfAnimationManager::InitAnimation()
	{
		for (size_t index = 0; index < _gltfModel->animations.size(); ++index)
		{
			std::shared_ptr<GltfAnimation> Animation = std::make_shared<GltfAnimation>(_gltfModel,_Model);
			Animation->InitAnimate(index);
			_Animations.push_back(Animation);

			_AnimationAllTime = (std::max)(_AnimationAllTime, Animation->GetAnimationTime());
		}

	}

	bool GltfAnimationManager::Play(float Second)
	{
		if (_AnimationAllTime < 0.001)
		{
			return false;
		}

		int nTmp = Second / _AnimationAllTime;

		float during = Second - nTmp * _AnimationAllTime;

		//during = frameCount * (1000.0f / 40.0f) / 1000.0f;
		_FrameCount++;

		for (int i = 0; i < _Animations.size(); i++)
		{
			_Animations[i]->Play(during, _Model);
			if (_Animations[i]->HasModelAnimatie())
			{
				_hasModelAnimate = true;
			}
		}
		return _hasModelAnimate;
	}


	bool GltfAnimationManager::HasAnimation() const
	{
		return _hasModelAnimate;
	}

}
