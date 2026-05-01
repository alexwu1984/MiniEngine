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
	class SceneTextures;
	struct DownSamplePSPrivate;

	class DownSamplePS
	{
	public:
		DownSamplePS(RenderCore::DynamicRHI* RHI);
		~DownSamplePS();

		void InitResource();
		void Draw(RenderCore::RHICommandContext& RHIContext, std::shared_ptr<SceneTextures> TargetBuffer);
		std::shared_ptr<RenderCore::RHIRenderTarget> GetDownSampleTarget();
	private:
		DownSamplePSPrivate* d_ptr = nullptr;
	};
}