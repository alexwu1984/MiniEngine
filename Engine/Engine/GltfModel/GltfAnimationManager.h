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
		bool Play(float Second);
		bool HasAnimation() const;
	private:
		bool ApplyAnimationTimeline(float Second);
		tinygltf::Model* _gltfModel = nullptr;
		GltfModel* _Model = nullptr;
		std::vector<std::shared_ptr<GltfAnimation>> _Animations;
		float _AnimationAllTime = 0;
		float _BlendShapeTimelineAllTime = 0;
		float _BlendShapeBlendTime = 0.05f;
		long _FrameCount = 0;
		bool _hasModelAnimate = false;
	};
}