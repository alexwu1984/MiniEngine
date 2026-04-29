#pragma once
#include "core/inc.h"
#include <memory>

namespace RenderCore
{
	class RHICommandContext;
	class DynamicRHI;
	class RHIViewPort;
	class RHITexture2D;
}

namespace Engine
{
	struct SSRProcessorPrivate;
	struct FSceneViewData;
	class GBuffer;

	class SSRProcessor
	{
	public:
		SSRProcessor(RenderCore::DynamicRHI* RHI);
		~SSRProcessor();

		void InitResource();
		void InvalidateTransientResources();
		void Draw(RenderCore::RHICommandContext& RHIContext, std::shared_ptr<GBuffer> TargetBuffer, std::shared_ptr<RenderCore::RHIViewPort> ViewPort,
				  std::shared_ptr<RenderCore::RHITexture2D> HistorySceneColor,
			      std::shared_ptr<const FSceneViewData> ViewData);
		std::shared_ptr<RenderCore::RHITexture2D> GetSSRBuffer() const;
	private:
		SSRProcessorPrivate* d_ptr = nullptr;
	};
}