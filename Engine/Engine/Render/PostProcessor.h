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
	struct PostProcessorPrivate;
	class GBuffer;
	class CameraComponent;

	class PostProcessor
	{
	public:
		PostProcessor(RenderCore::DynamicRHI* RHI);
		~PostProcessor();

		void LoadConfig(const std::wstring& FileName);
		void InitResource();
		void Draw(RenderCore::RHICommandContext& RHIContext, std::shared_ptr<GBuffer> TargetBuffer, 
				  std::shared_ptr<RenderCore::RHIViewPort> ViewPort, std::shared_ptr<CameraComponent> Camera);
		std::shared_ptr<RenderCore::RHITexture2D> GetSSRBuffer() const;
	private:
		void Tonemapping(RenderCore::RHICommandContext& RHIContext, std::shared_ptr<GBuffer> TargetBuffer,std::shared_ptr<RenderCore::RHIViewPort> ViewPort);
		void ApplyBloom(RenderCore::RHICommandContext& RHIContext, std::shared_ptr<GBuffer> TargetBuffer);
		void ApplySSR(RenderCore::RHICommandContext& RHIContext, std::shared_ptr<GBuffer> TargetBuffer);
	private:
		PostProcessorPrivate* d_ptr = nullptr;
	};
}