#include "Render/Shadow/ShadowMapManager.h"
#include "Render/Shadow/ShadowMap.h"

namespace Engine
{
	struct ShadowMapManagerPrivate
	{
		std::vector<std::shared_ptr<ShadowMap>> cascadeShadowMaps;
	};

	ShadowMapManager::ShadowMapManager()
		:d_ptr(new ShadowMapManagerPrivate())
	{

	}

	ShadowMapManager::~ShadowMapManager()
	{
		delete d_ptr;
	}

	void ShadowMapManager::SetShadowCascades(size_t cascades)
	{
		if (cascades > CONFIG_MAX_SHADOW_CASCADES)
			return;

		C_P(ShadowMapManager);
		for (int ni = 0; ni < cascades; ni++)
		{
			d->cascadeShadowMaps.emplace_back(std::make_shared<ShadowMap>());
		}
	}

	void ShadowMapManager::Update(std::shared_ptr<SceneView> sceneView)
	{
		UpdateCascadeShadowMaps(sceneView);
	}


	std::shared_ptr<Engine::ShadowMap> ShadowMapManager::GetShadowMap(int32_t index) const
	{
		C_P(ShadowMapManager);
		if (index > d->cascadeShadowMaps.size())
		{
			return {};
		}

		return d->cascadeShadowMaps[index];
	}

	void ShadowMapManager::UpdateCascadeShadowMaps(std::shared_ptr<SceneView> sceneView)
	{
		C_P(ShadowMapManager);
		CascadeParameters cascadeParams;
		ShadowMap::ComputeSceneCascadeParams(sceneView, cascadeParams);
		assert(d->cascadeShadowMaps.size() > 0);
		d->cascadeShadowMaps[0]->Update(cascadeParams);
	}


}