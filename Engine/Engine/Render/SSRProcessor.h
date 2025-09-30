#pragma once
#include "core/inc.h"

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
	class GBuffer;
	class CameraComponent;

	class SSRProcessor
	{
	public:
		SSRProcessor(RenderCore::DynamicRHI* RHI);
		~SSRProcessor();

		void InitResource();
		void Draw(RenderCore::RHICommandContext& RHIContext, std::shared_ptr<RenderCore::RHIViewPort> ViewPort,
			     std::shared_ptr<GBuffer> TargetBuffer,std::shared_ptr<RenderCore::RHITexture2D> HistorySceneColor,
				 std::shared_ptr<CameraComponent> Camera);
	private:
		SSRProcessorPrivate* d_ptr = nullptr;
	};
}