#include "Render/PostProcessPass.h"
#include "RHI/RHICachedStates.h"
#include "RHI/RHICommandContext.h"
#include "RHI/RHIPipeLineState.h"
#include "RHI/RHIShdader.h"
#include "RHI/RHIViewPort.h"
#include "Render/FXAA.h"
#include "Render/GBuffer.h"
#include "Render/SSRProcessor.h"
#include "Render/TemporalAA.h"
#include "Scene/CameraComponent.h"

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

	SSRPass::SSRPass(RenderCore::RHICommandContext& InRHIContext, std::shared_ptr<GBuffer> InTargetBuffer,
					 std::shared_ptr<RenderCore::RHIViewPort> InViewPort, std::shared_ptr<CameraComponent> InCamera,
					 std::shared_ptr<SSRProcessor> InSSR,
					 std::function<std::shared_ptr<RenderCore::RHITexture2D>()> InReflectionColor)
		: RHIContext(InRHIContext)
		, TargetBuffer(std::move(InTargetBuffer))
		, ViewPort(std::move(InViewPort))
		, Camera(std::move(InCamera))
		, SSR(std::move(InSSR))
		, ReflectionColor(std::move(InReflectionColor))
	{
	}

	RenderPassDesc SSRPass::BuildDesc() const
	{
		return {
			"SSR",
			{
				{ "Normal", [TargetBuffer = TargetBuffer]() { return TargetBuffer->GetNormalBuffer(); } },
				{ "MetallicRoughness", [TargetBuffer = TargetBuffer]() { return TargetBuffer->GetMetallicRoughnessBuffer(); } },
				{ "Depth", [TargetBuffer = TargetBuffer]() { return TargetBuffer->GetDepth(); } },
				{ "ReflectionColor", ReflectionColor }
			},
			{
				{ "SSRBuffer", [SSR = SSR]() { return SSR ? SSR->GetSSRBuffer() : std::shared_ptr<RenderCore::RHITexture2D>{}; }, false }
			},
			[Pass = *this]() { Pass.Execute(); }
		};
	}

	void SSRPass::Execute() const
	{
		SSR->Draw(RHIContext, TargetBuffer, ViewPort, ReflectionColor(), Camera);
	}

	BloomPass::BloomPass(RenderCore::RHICommandContext& InRHIContext, std::shared_ptr<GBuffer> InTargetBuffer,
						 std::shared_ptr<RenderCore::RHIViewPort> InViewPort, std::shared_ptr<Bloom> InBloomEffect,
						 std::function<std::shared_ptr<RenderCore::RHITexture2D>()> InSourceTexture)
		: RHIContext(InRHIContext)
		, TargetBuffer(std::move(InTargetBuffer))
		, ViewPort(std::move(InViewPort))
		, BloomEffect(std::move(InBloomEffect))
		, SourceTexture(std::move(InSourceTexture))
	{
	}

	RenderPassDesc BloomPass::BuildDesc() const
	{
		return {
			"Bloom",
			{
				{ "SourceColor", SourceTexture }
			},
			{
				{ "BloomResult", [BloomEffect = BloomEffect]() { return BloomEffect ? BloomEffect->GetResult() : std::shared_ptr<RenderCore::RHITexture2D>{}; }, false }
			},
			[Pass = *this]() { Pass.Execute(); }
		};
	}

	void BloomPass::Execute() const
	{
		ViewPort->SetRenderTarget();
		RHIContext.SetViewPort(0, 0, ViewPort->GetSize().x, ViewPort->GetSize().y);
		BloomEffect->Draw(RHIContext, TargetBuffer);
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

	TAAPass::TAAPass(RenderCore::RHICommandContext& InRHIContext, std::shared_ptr<GBuffer> InTargetBuffer,
					 std::shared_ptr<RenderCore::RHIViewPort> InViewPort, std::shared_ptr<CameraComponent> InCamera,
					 std::shared_ptr<TemporallAA> InTAA,
					 std::function<std::shared_ptr<RenderCore::RHITexture2D>()> InSourceTexture)
		: RHIContext(InRHIContext)
		, TargetBuffer(std::move(InTargetBuffer))
		, ViewPort(std::move(InViewPort))
		, Camera(std::move(InCamera))
		, TAA(std::move(InTAA))
		, SourceTexture(std::move(InSourceTexture))
	{
	}

	RenderPassDesc TAAPass::BuildDesc() const
	{
		return {
			"TAA",
			{
				{ "SceneColorWithBloom", SourceTexture },
				{ "MotionVector", [TargetBuffer = TargetBuffer]() { return TargetBuffer->GetMotionVector(); } },
				{ "Depth", [TargetBuffer = TargetBuffer]() { return TargetBuffer->GetDepth(); } }
			},
			{
				{ "SceneColor", [TargetBuffer = TargetBuffer]() { return TargetBuffer->GetSceneColor(); } }
			},
			[Pass = *this]() { Pass.Execute(); }
		};
	}

	void TAAPass::Execute() const
	{
		ViewPort->SetRenderTarget();
		RHIContext.SetViewPort(0, 0, ViewPort->GetSize().x, ViewPort->GetSize().y);
		TAA->Draw(RHIContext, TargetBuffer, Camera);
	}

	FXAAPass::FXAAPass(RenderCore::RHICommandContext& InRHIContext, std::shared_ptr<RenderCore::RHIViewPort> InViewPort,
					   std::shared_ptr<RenderCore::FXAA> InFXAA,
					   std::function<std::shared_ptr<RenderCore::RHITexture2D>()> InSourceTexture)
		: RHIContext(InRHIContext)
		, ViewPort(std::move(InViewPort))
		, FXAA(std::move(InFXAA))
		, SourceTexture(std::move(InSourceTexture))
	{
	}

	RenderPassDesc FXAAPass::BuildDesc() const
	{
		return {
			"FXAA",
			{
				{ "SceneColorWithBloom", SourceTexture }
			},
			{
				{ "FXAAResult", [FXAA = FXAA]() { return FXAA ? FXAA->GetResult() : std::shared_ptr<RenderCore::RHITexture2D>{}; }, false }
			},
			[Pass = *this]() { Pass.Execute(); }
		};
	}

	void FXAAPass::Execute() const
	{
		ViewPort->SetRenderTarget();
		RHIContext.SetViewPort(0, 0, ViewPort->GetSize().x, ViewPort->GetSize().y);
		FXAA->Draw(RHIContext, SourceTexture());
	}
}
