#pragma once
#include "core/inc.h"

namespace RenderCore
{
	class RHICommandContext;
	class DynamicRHI;
	class RHITexture2D;
}

namespace Engine
{
	struct TemporallAAPrivate;
	struct FSceneViewData;
	class FSceneTextures;

	class TemporallAA
	{
	public:
		TemporallAA(RenderCore::DynamicRHI* RHI);
		~TemporallAA();

		void InitResource();
		void InvalidateTransientResources();
		void Draw(RenderCore::RHICommandContext& RHIContext, std::shared_ptr<FSceneTextures> SceneTextures, std::shared_ptr<const FSceneViewData> ViewData);
		std::shared_ptr<RenderCore::RHITexture2D> GetHistoryBuffer();
	private:
		TemporallAAPrivate* d_ptr = nullptr;
	};
}