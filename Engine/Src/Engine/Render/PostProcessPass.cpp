#include "Render/PostProcessPass.h"
#include "RHI/RHICachedStates.h"
#include "RHI/RHICommandContext.h"
#include "RHI/RHIPipeLineState.h"
#include "RHI/RHIShdader.h"
#include "RHI/RHIViewPort.h"
#include "Render/GBuffer.h"

namespace Engine
{
	namespace
	{
		RenderCore::GraphicsPipelineStateInitializer CreateFullscreenPipelineState(
			const FullscreenPostProcessPassResources& Resources,
			std::shared_ptr<RenderCore::RHIPixelShader> PixelShader)
		{
			RenderCore::GraphicsPipelineStateInitializer Init;
			Init.VertexShader = Resources.VertexShader;
			Init.PixelShader = PixelShader;
			Init.BlendState = RenderCore::RHICachedStates::BlendTraditional;
			Init.DepthStencilState = RenderCore::RHICachedStates::DepthStateDisable;
			Init.RasterizerState = RenderCore::RHICachedStates::RasterizerStateCullNone;
			return Init;
		}
	}

	TonemappingPass::TonemappingPass(RenderCore::RHICommandContext& InRHIContext, std::shared_ptr<GBuffer> InTargetBuffer,
									 std::shared_ptr<RenderCore::RHIViewPort> InViewPort,
									 FullscreenPostProcessPassResources InResources,
									 std::function<std::shared_ptr<RenderCore::RHITexture2D>()> InSourceTexture)
		: RHIContext(InRHIContext)
		, TargetBuffer(std::move(InTargetBuffer))
		, ViewPort(std::move(InViewPort))
		, Resources(std::move(InResources))
		, SourceTexture(std::move(InSourceTexture))
	{
	}

	RenderPassDesc TonemappingPass::BuildDesc() const
	{
		return {
			"Tonemapping",
			{
				{ "SourceColor", SourceTexture }
			},
			{},
			[Pass = *this]() { Pass.Execute(); }
		};
	}

	void TonemappingPass::Execute() const
	{
		RenderCore::RHICommandMark Mark(RHIContext, "Tonemapping");
		RHIContext.RHISetGraphicsPipelineState(CreateFullscreenPipelineState(Resources, Resources.TonemappingShader));
		ViewPort->SetRenderTarget();
		RHIContext.SetViewPort(0, 0, ViewPort->GetSize().x, ViewPort->GetSize().y);
		RHIContext.RHISetShaderSampler(RenderCore::SF_Pixel, 0, RenderCore::RHICachedStates::ClampLinerSampler);
		RHIContext.RHISetShaderTexture(RenderCore::SF_Pixel, 0, SourceTexture());
		RHIContext.Draw(3);
	}

	ApplyBloomPass::ApplyBloomPass(RenderCore::RHICommandContext& InRHIContext, std::shared_ptr<GBuffer> InTargetBuffer,
								   std::shared_ptr<RenderCore::RHIViewPort> InViewPort,
								   FullscreenPostProcessPassResources InResources,
								   std::function<std::shared_ptr<RenderCore::RHITexture2D>()> InSourceTexture,
								   std::function<std::shared_ptr<RenderCore::RHITexture2D>()> InBloomTexture)
		: RHIContext(InRHIContext)
		, TargetBuffer(std::move(InTargetBuffer))
		, ViewPort(std::move(InViewPort))
		, Resources(std::move(InResources))
		, SourceTexture(std::move(InSourceTexture))
		, BloomTexture(std::move(InBloomTexture))
	{
	}

	RenderPassDesc ApplyBloomPass::BuildDesc() const
	{
		return {
			"ApplyBloom",
			{
				{ "SourceColor", SourceTexture },
				{ "BloomResult", BloomTexture }
			},
			{
				{ "SceneColorWithBloom", [TargetBuffer = TargetBuffer]() { return TargetBuffer->GetSceneColorWithBloom(); } }
			},
			[Pass = *this]() { Pass.Execute(); }
		};
	}

	void ApplyBloomPass::Execute() const
	{
		if (!BloomTexture())
			return;

		RenderCore::RHICommandMark Mark(RHIContext, "ApplyBloom");
		RHIContext.RHISetGraphicsPipelineState(CreateFullscreenPipelineState(Resources, Resources.ApplyBloomShader));
		ViewPort->SetRenderTarget();
		RHIContext.SetViewPort(0, 0, ViewPort->GetSize().x, ViewPort->GetSize().y);
		RHIContext.SetRenderTarget(TargetBuffer->GetSceneColorWithBloom(), nullptr);
		RHIContext.RHISetShaderSampler(RenderCore::SF_Pixel, 0, RenderCore::RHICachedStates::ClampLinerSampler);
		RHIContext.RHISetShaderTexture(RenderCore::SF_Pixel, 0, SourceTexture());
		RHIContext.RHISetShaderTexture(RenderCore::SF_Pixel, 1, BloomTexture());

		if (Resources.BloomConstants)
		{
			Resources.BloomConstants->Data.BloomIntensity = 1.0f;
			Resources.BloomConstants->UpdateUniformBuffer();
			Resources.BloomConstants->SetShaderUniformBuffer(RenderCore::EShaderFrequency::SF_Pixel);
		}

		RHIContext.Draw(3);
	}

	ApplySSRPass::ApplySSRPass(RenderCore::RHICommandContext& InRHIContext, std::shared_ptr<GBuffer> InTargetBuffer,
							   std::shared_ptr<RenderCore::RHIViewPort> InViewPort,
							   FullscreenPostProcessPassResources InResources,
							   std::function<std::shared_ptr<RenderCore::RHITexture2D>()> InSSRTexture)
		: RHIContext(InRHIContext)
		, TargetBuffer(std::move(InTargetBuffer))
		, ViewPort(std::move(InViewPort))
		, Resources(std::move(InResources))
		, SSRTexture(std::move(InSSRTexture))
	{
	}

	RenderPassDesc ApplySSRPass::BuildDesc() const
	{
		return {
			"ApplySSR",
			{
				{ "SceneColor", [TargetBuffer = TargetBuffer]() { return TargetBuffer->GetSceneColor(); } },
				{ "SSRBuffer", SSRTexture }
			},
			{
				{ "SceneColorWithSSR", [TargetBuffer = TargetBuffer]() { return TargetBuffer->GetSceneColorWithSSR(); } }
			},
			[Pass = *this]() { Pass.Execute(); }
		};
	}

	void ApplySSRPass::Execute() const
	{
		if (!SSRTexture())
			return;

		RenderCore::RHICommandMark Mark(RHIContext, "ApplySSR");
		RHIContext.RHISetGraphicsPipelineState(CreateFullscreenPipelineState(Resources, Resources.ApplySSRShader));
		ViewPort->SetRenderTarget();
		RHIContext.SetViewPort(0, 0, ViewPort->GetSize().x, ViewPort->GetSize().y);
		RHIContext.SetRenderTarget(TargetBuffer->GetSceneColorWithSSR(), nullptr);
		RHIContext.RHISetShaderSampler(RenderCore::SF_Pixel, 0, RenderCore::RHICachedStates::ClampLinerSampler);
		RHIContext.RHISetShaderTexture(RenderCore::SF_Pixel, 0, TargetBuffer->GetSceneColor());
		RHIContext.RHISetShaderTexture(RenderCore::SF_Pixel, 1, SSRTexture());
		RHIContext.Draw(3);
	}
}
