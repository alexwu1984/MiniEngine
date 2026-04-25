#pragma once
#include "core/inc.h"
#include "Render/Bloom.h"
#include "Render/PostProcessGraph.h"

namespace RenderCore
{
	class RHICommandContext;
	class RHIPixelShader;
	class RHITexture2D;
	class RHIVertexShader;
	class RHIViewPort;
}

namespace Engine
{
	class GBuffer;

	struct FullscreenPostProcessPassResources
	{
		std::shared_ptr<RenderCore::RHIVertexShader> VertexShader;
		std::shared_ptr<RenderCore::RHIPixelShader> TonemappingShader;
		std::shared_ptr<RenderCore::RHIPixelShader> ApplyBloomShader;
		std::shared_ptr<RenderCore::RHIPixelShader> ApplySSRShader;
		BloomContantsWrap* BloomConstants = nullptr;
	};

	class TonemappingPass
	{
	public:
		TonemappingPass(RenderCore::RHICommandContext& RHIContext, std::shared_ptr<GBuffer> TargetBuffer,
						std::shared_ptr<RenderCore::RHIViewPort> ViewPort,
						FullscreenPostProcessPassResources Resources,
						std::function<std::shared_ptr<RenderCore::RHITexture2D>()> SourceTexture);

		RenderPassDesc BuildDesc() const;

	private:
		void Execute() const;

		RenderCore::RHICommandContext& RHIContext;
		std::shared_ptr<GBuffer> TargetBuffer;
		std::shared_ptr<RenderCore::RHIViewPort> ViewPort;
		FullscreenPostProcessPassResources Resources;
		std::function<std::shared_ptr<RenderCore::RHITexture2D>()> SourceTexture;
	};

	class ApplyBloomPass
	{
	public:
		ApplyBloomPass(RenderCore::RHICommandContext& RHIContext, std::shared_ptr<GBuffer> TargetBuffer,
					   std::shared_ptr<RenderCore::RHIViewPort> ViewPort,
					   FullscreenPostProcessPassResources Resources,
					   std::function<std::shared_ptr<RenderCore::RHITexture2D>()> SourceTexture,
					   std::function<std::shared_ptr<RenderCore::RHITexture2D>()> BloomTexture);

		RenderPassDesc BuildDesc() const;

	private:
		void Execute() const;

		RenderCore::RHICommandContext& RHIContext;
		std::shared_ptr<GBuffer> TargetBuffer;
		std::shared_ptr<RenderCore::RHIViewPort> ViewPort;
		FullscreenPostProcessPassResources Resources;
		std::function<std::shared_ptr<RenderCore::RHITexture2D>()> SourceTexture;
		std::function<std::shared_ptr<RenderCore::RHITexture2D>()> BloomTexture;
	};

	class ApplySSRPass
	{
	public:
		ApplySSRPass(RenderCore::RHICommandContext& RHIContext, std::shared_ptr<GBuffer> TargetBuffer,
					 std::shared_ptr<RenderCore::RHIViewPort> ViewPort,
					 FullscreenPostProcessPassResources Resources,
					 std::function<std::shared_ptr<RenderCore::RHITexture2D>()> SSRTexture);

		RenderPassDesc BuildDesc() const;

	private:
		void Execute() const;

		RenderCore::RHICommandContext& RHIContext;
		std::shared_ptr<GBuffer> TargetBuffer;
		std::shared_ptr<RenderCore::RHIViewPort> ViewPort;
		FullscreenPostProcessPassResources Resources;
		std::function<std::shared_ptr<RenderCore::RHITexture2D>()> SSRTexture;
	};
}
