#pragma once
#include "core/inc.h"
#include "tinygltf/tiny_gltf.h"

namespace Engine
{
	class GltfAnimation;
	class GltfModel;

	class GltfAnimationManager
	{
	public:
		GltfAnimationManager(tinygltf::Model* gltfModel, GltfModel* Model);
		~GltfAnimationManager();

		void InitAnimation();
		void Play(float Second);

	private:
		tinygltf::Model* _gltfModel = nullptr;
		GltfModel* _Model = nullptr;
		std::vector<std::shared_ptr<GltfAnimation>> _Animations;
		float _AnimationAllTime = 0;
		long _FrameCount = 0;
		bool _hasModelAnimate = false;
	};
}