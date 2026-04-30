#pragma once
#include "core/inc.h"
#include "Render/MaterialPreFrame.h"
#include <memory>
#include <vector>

namespace Engine
{
	struct FShadowProjectorSceneData;
	class ShadowMap;
	//Ä¿Ç°»¹Ã»ÕýÊ½ÊµÏÖÁª¼¶ÒõÓ°
	constexpr size_t CONFIG_MAX_SHADOW_CASCADES = 1;

	struct ShadowMapManagerPrivate;

	class ShadowMapManager
	{
	public:
		ShadowMapManager();
		~ShadowMapManager();

		//Ö»ÔÊÐíµ÷ÓÃÒ»´Î
		void SetShadowCascades(size_t cascades);
		void Update(const std::vector<Light>& lights, const FShadowProjectorSceneData& ProjectorScene);

		std::shared_ptr<ShadowMap> GetShadowMap(int32_t index) const;

	private:
		void UpdateCascadeShadowMaps(const std::vector<Light>& lights, const FShadowProjectorSceneData& ProjectorScene);
	private:
		ShadowMapManagerPrivate* d_ptr = nullptr;
	};
}