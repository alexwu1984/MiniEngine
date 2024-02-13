#pragma once
#include "core/inc.h"

namespace Engine
{
	class SceneView;
	class ShadowMap;
	//目前还没正式实现联级阴影
	constexpr size_t CONFIG_MAX_SHADOW_CASCADES = 1;

	struct ShadowMapManagerPrivate;

	class ShadowMapManager
	{
	public:
		ShadowMapManager();
		~ShadowMapManager();

		//只允许调用一次
		void SetShadowCascades(size_t cascades);
		void Update(std::shared_ptr<SceneView> sceneView);

		std::shared_ptr<ShadowMap> GetShadowMap(int32_t index) const;

	private:
		void UpdateCascadeShadowMaps(std::shared_ptr<SceneView> sceneView);
	private:
		ShadowMapManagerPrivate* d_ptr = nullptr;
	};
}