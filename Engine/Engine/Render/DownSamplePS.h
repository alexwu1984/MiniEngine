#pragma once
#include "core/inc.h"

namespace RenderCore
{
	class RHICommandContext;
	class DynamicRHI;
	class RHIRenderTarget;
}

namespace Engine
{
	class FSceneTextures;
	struct DownSamplePSPrivate;

	class DownSamplePS
	{
	public:
		DownSamplePS(RenderCore::DynamicRHI* RHI);
		~DownSamplePS();

		void InitResource();
		void Draw(RenderCore::RHICommandContext& RHIContext, std::shared_ptr<FSceneTextures> SceneTextures);
		std::shared_ptr<RenderCore::RHIRenderTarget> GetDownSampleTarget();
	private:
		DownSamplePSPrivate* d_ptr = nullptr;
	};
}