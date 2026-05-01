#include "Render/PostProcessPass.h"
#include "core/system.h"
#include "RHI/DynamicRHI.h"
#include "RHI/RHICachedStates.h"
#include "RHI/RHICommandContext.h"
#include "RHI/RHIPipeLineState.h"
#include "RHI/RHIShdader.h"
#include "RHI/RHIViewPort.h"
#include "Render/FXAA.h"
#include "Render/GBuffer.h"
#include "Render/SSRProcessor.h"
#include "Render/TemporalAA.h"
#include "Render/SceneRendering/FSceneViewData.h"

namespace Engine
{
	namespace
	{
		RenderCore::GraphicsPipelineStateInitializer CreateFullscreenPipelineState(
			std::shared_ptr<RenderCore::RHIVertexShader> VertexShader,
			std::shared_ptr<RenderCore::RHIPixelShader> PixelShader)
		{
			RenderCore::GraphicsPipelineStateInitializer Init;
			Init.VertexShader = VertexShader;
			Init.PixelShader = PixelShader;
			Init.BlendState = RenderCore::RHICachedStates::BlendTraditional;
			Init.DepthStencilState = RenderCore::RHICachedStates::DepthStateDisable;
			Init.RasterizerState = RenderCore::RHICachedStates::RasterizerStateCullNone;
			return Init;
		}

		std::wstring GetPostProcessShaderPath()
		{
			return core::process_directory().wstring() + L"/ShaderLibDX/PostProcess.hlsl";
		}
	}

	TonemappingPass::TonemappingPass(RenderCore::DynamicRHI* InRHI, std::shared_ptr<RenderCore::RHIVertexShader> InVertexShader,
									 BloomContantsWrap* InBloomConstants)
		: RHI(InRHI)
		, VertexShader(std::move(InVertexShader))
		, BloomConstants(InBloomConstants)
	{
	}

	void TonemappingPass::InitResource()
	{
		PixelShader = RHI->RHICreatePixelShader(GetPostProcessShaderPath(), "PS_Tonemapping", {});
	}

	ApplyBloomPass::ApplyBloomPass(RenderCore::DynamicRHI* InRHI, std::shared_ptr<RenderCore::RHIVertexShader> InVertexShader,
								   BloomContantsWrap* InBloomConstants)
		: RHI(InRHI)
		, VertexShader(std::move(InVertexShader))
		, BloomConstants(InBloomConstants)
	{
	}

	void ApplyBloomPass::InitResource()
	{
		PixelShader = RHI->RHICreatePixelShader(GetPostProcessShaderPath(), "PS_ApplyBloom", {});
	}

	ApplySSRPass::ApplySSRPass(RenderCore::DynamicRHI* InRHI, std::shared_ptr<RenderCore::RHIVertexShader> InVertexShader)
		: RHI(InRHI)
		, VertexShader(std::move(InVertexShader))
	{
	}

	void ApplySSRPass::InitResource()
	{
		PixelShader = RHI->RHICreatePixelShader(GetPostProcessShaderPath(), "PS_ApplySSR", {});
	}

	RenderPassDesc TonemappingPass::BuildDesc(RenderCore::RHICommandContext& RHIContext, std::shared_ptr<GBuffer> TargetBuffer,
											 std::shared_ptr<RenderCore::RHIViewPort> ViewPort,
											 std::function<std::shared_ptr<RenderCore::RHITexture2D>()> SourceTexture,
											 const std::string& SourceResourceName) const
	{
		return {
			"Tonemapping",
			{
				{ SourceResourceName, SourceTexture }
			},
			{},
			[this, &RHIContext, TargetBuffer, ViewPort, SourceTexture]() { Execute(RHIContext, TargetBuffer, ViewPort, SourceTexture); }
		};
	}

	void TonemappingPass::Execute(RenderCore::RHICommandContext& RHIContext, std::shared_ptr<GBuffer> TargetBuffer,
								  std::shared_ptr<RenderCore::RHIViewPort> ViewPort,
								  std::function<std::shared_ptr<RenderCore::RHITexture2D>()> SourceTexture) const
	{
		RenderCore::RHICommandMark Mark(RHIContext, "Tonemapping");
		ViewPort->SetRenderTarget();
		RHIContext.RHISetGraphicsPipelineState(CreateFullscreenPipelineState(VertexShader, PixelShader));
		RHIContext.SetViewPort(0, 0, ViewPort->GetSize().x, ViewPort->GetSize().y);
		if (BloomConstants)
		{
			BloomConstants->UpdateUniformBuffer();
			BloomConstants->SetShaderUniformBuffer(RenderCore::SF_Pixel);
		}
		RHIContext.RHISetShaderSampler(RenderCore::SF_Pixel, 0, RenderCore::RHICachedStates::ClampLinerSampler);
		RHIContext.RHISetShaderTexture(RenderCore::SF_Pixel, 0, SourceTexture());
		RHIContext.Draw(3);
	}

	SSRPass::SSRPass(RenderCore::RHICommandContext& InRHIContext, std::shared_ptr<GBuffer> InTargetBuffer,
					 std::shared_ptr<RenderCore::RHIViewPort> InViewPort, std::shared_ptr<const FSceneViewData> InViewData,
					 std::shared_ptr<SSRProcessor> InSSR,
					 std::function<std::shared_ptr<RenderCore::RHITexture2D>()> InReflectionColor)
		: RHIContext(InRHIContext)
		, TargetBuffer(std::move(InTargetBuffer))
		, ViewPort(std::move(InViewPort))
		, ViewData(std::move(InViewData))
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
		SSR->Draw(RHIContext, TargetBuffer, ViewPort, ReflectionColor(), ViewData);
	}

	BloomPass::BloomPass(RenderCore::RHICommandContext& InRHIContext, std::shared_ptr<GBuffer> InTargetBuffer,
						 std::shared_ptr<RenderCore::RHIViewPort> InViewPort, std::shared_ptr<Bloom> InBloomEffect,
						 std::function<std::shared_ptr<RenderCore::RHITexture2D>()> InSourceTexture,
						 std::string InSceneColorDependencyName)
		: RHIContext(InRHIContext)
		, TargetBuffer(std::move(InTargetBuffer))
		, ViewPort(std::move(InViewPort))
		, BloomEffect(std::move(InBloomEffect))
		, SourceTexture(std::move(InSourceTexture))
		, SceneColorDependencyName(std::move(InSceneColorDependencyName))
	{
	}

	RenderPassDesc BloomPass::BuildDesc() const
	{
		return {
			"Bloom",
			{
				{ SceneColorDependencyName, SourceTexture }
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

	RenderPassDesc ApplyBloomPass::BuildDesc(RenderCore::RHICommandContext& RHIContext, std::shared_ptr<GBuffer> TargetBuffer,
											 std::shared_ptr<RenderCore::RHIViewPort> ViewPort, const std::string& SceneColorDependencyName,
											 std::function<std::shared_ptr<RenderCore::RHITexture2D>()> SourceTexture,
											 std::function<std::shared_ptr<RenderCore::RHITexture2D>()> BloomTexture) const
	{
		return {
			"ApplyBloom",
			{
				{ SceneColorDependencyName, SourceTexture },
				{ "BloomResult", BloomTexture }
			},
			{
				{ "SceneColorWithBloom", [TargetBuffer = TargetBuffer]() { return TargetBuffer->GetSceneColorWithBloom(); } }
			},
			[this, &RHIContext, TargetBuffer, ViewPort, SourceTexture, BloomTexture]() { Execute(RHIContext, TargetBuffer, ViewPort, SourceTexture, BloomTexture); }
		};
	}

	void ApplyBloomPass::Execute(RenderCore::RHICommandContext& RHIContext, std::shared_ptr<GBuffer> TargetBuffer,
								 std::shared_ptr<RenderCore::RHIViewPort> ViewPort,
								 std::function<std::shared_ptr<RenderCore::RHITexture2D>()> SourceTexture,
								 std::function<std::shared_ptr<RenderCore::RHITexture2D>()> BloomTexture) const
	{
		if (!BloomTexture())
			return;

		RenderCore::RHICommandMark Mark(RHIContext, "ApplyBloom");
		ViewPort->SetRenderTarget();
		RHIContext.RHISetGraphicsPipelineState(CreateFullscreenPipelineState(VertexShader, PixelShader));
		RHIContext.SetViewPort(0, 0, ViewPort->GetSize().x, ViewPort->GetSize().y);
		RHIContext.SetRenderTarget(TargetBuffer->GetSceneColorWithBloom(), nullptr);
		RHIContext.RHISetShaderSampler(RenderCore::SF_Pixel, 0, RenderCore::RHICachedStates::ClampLinerSampler);
		RHIContext.RHISetShaderTexture(RenderCore::SF_Pixel, 0, SourceTexture());
		RHIContext.RHISetShaderTexture(RenderCore::SF_Pixel, 1, BloomTexture());

		if (BloomConstants)
		{
			BloomConstants->Data.BloomIntensity = 1.0f;
			BloomConstants->UpdateUniformBuffer();
			BloomConstants->SetShaderUniformBuffer(RenderCore::EShaderFrequency::SF_Pixel);
		}

		RHIContext.Draw(3);
	}

	RenderPassDesc ApplySSRPass::BuildDesc(RenderCore::RHICommandContext& RHIContext, std::shared_ptr<GBuffer> TargetBuffer,
										   std::shared_ptr<RenderCore::RHIViewPort> ViewPort,
										   std::function<std::shared_ptr<RenderCore::RHITexture2D>()> SSRTexture) const
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
			[this, &RHIContext, TargetBuffer, ViewPort, SSRTexture]() { Execute(RHIContext, TargetBuffer, ViewPort, SSRTexture); }
		};
	}

	void ApplySSRPass::Execute(RenderCore::RHICommandContext& RHIContext, std::shared_ptr<GBuffer> TargetBuffer,
							   std::shared_ptr<RenderCore::RHIViewPort> ViewPort,
							   std::function<std::shared_ptr<RenderCore::RHITexture2D>()> SSRTexture) const
	{
		if (!SSRTexture())
			return;

		RenderCore::RHICommandMark Mark(RHIContext, "ApplySSR");
		ViewPort->SetRenderTarget();
		RHIContext.RHISetGraphicsPipelineState(CreateFullscreenPipelineState(VertexShader, PixelShader));
		RHIContext.SetViewPort(0, 0, ViewPort->GetSize().x, ViewPort->GetSize().y);
		RHIContext.SetRenderTarget(TargetBuffer->GetSceneColorWithSSR(), nullptr);
		RHIContext.RHISetShaderSampler(RenderCore::SF_Pixel, 0, RenderCore::RHICachedStates::ClampLinerSampler);
		RHIContext.RHISetShaderTexture(RenderCore::SF_Pixel, 0, TargetBuffer->GetSceneColor());
		RHIContext.RHISetShaderTexture(RenderCore::SF_Pixel, 1, SSRTexture());
		RHIContext.Draw(3);
	}

	TAAPass::TAAPass(RenderCore::RHICommandContext& InRHIContext, std::shared_ptr<GBuffer> InTargetBuffer,
					 std::shared_ptr<RenderCore::RHIViewPort> InViewPort, std::shared_ptr<const FSceneViewData> InViewData,
					 std::shared_ptr<TemporallAA> InTAA,
					 std::function<std::shared_ptr<RenderCore::RHITexture2D>()> InSourceTexture)
		: RHIContext(InRHIContext)
		, TargetBuffer(std::move(InTargetBuffer))
		, ViewPort(std::move(InViewPort))
		, ViewData(std::move(InViewData))
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
		TAA->Draw(RHIContext, TargetBuffer, ViewData);
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
