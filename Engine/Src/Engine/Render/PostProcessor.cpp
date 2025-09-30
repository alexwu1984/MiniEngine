#include "Render/PostProcessor.h"
#include "core/system.h"
#include "RHI/RHIShdader.h"
#include "RHI/RHIShaderDefine.h"
#include "RHI/RHIPipeLineState.h"
#include "RHI/RHICachedStates.h"
#include "RHI/DynamicRHI.h"
#include "RHI/RHIViewPort.h"
#include "Engine/Engine.h"
#include "Render/GBuffer.h"
#include "Render/TemporalAA.h"
#include "Render/Bloom.h"
#include "Render/RenderUtil.h"
#include "Render/SSRProcessor.h"

namespace Engine
{
	using namespace RenderCore;
	struct PostProcessorPrivate
	{
		DynamicRHI* RHI = nullptr;
		std::shared_ptr<RHIVertexShader> VertexShader;
		std::shared_ptr<RHIPixelShader> PixelShader;
		std::shared_ptr<RHIPixelShader> AppalyBloomShader;
		std::shared_ptr<TemporallAA> TAA;
		std::shared_ptr<Bloom> BloomEffect;

		PostProcessorPrivate(DynamicRHI* _RHI) :
			GET_SHADER_STRUCT_MEMBER(BloomContants)(_RHI), RHI(_RHI)
		{

		}
		DECLARE_SHADER_STRUCT_MEMBER(BloomContants);
	};

	PostProcessor::PostProcessor(RenderCore::DynamicRHI* RHI)
		:d_ptr(new PostProcessorPrivate(RHI))
	{
		C_P(PostProcessor);
	}

	PostProcessor::~PostProcessor()
	{
		delete d_ptr;
	}

	void PostProcessor::InitResource()
	{
		C_P(PostProcessor);
		std::wstring ShaderPath = core::process_directory().wstring() + L"/ShaderLibDX/";
		ShaderPath += L"PostProcess.hlsl";

		d->VertexShader = d->RHI->RHICreateVertexShader(ShaderPath, "VS_ScreenQuad", {}, {});
		d->PixelShader = d->RHI->RHICreatePixelShader(ShaderPath, "PS_Tonemapping", {});
		d->AppalyBloomShader = d->RHI->RHICreatePixelShader(ShaderPath, "PS_ApplyBloom", {});

		d->TAA = std::make_shared<TemporallAA>(d->RHI);
		d->TAA->InitResource();
		d->BloomEffect = std::make_shared<Bloom>(d->RHI);
		d->BloomEffect->InitResource();
	}

	void PostProcessor::Draw(RenderCore::RHICommandContext& RHIContext, std::shared_ptr<GBuffer> TargetBuffer, 
						     std::shared_ptr<RHIViewPort> ViewPort, std::shared_ptr<CameraComponent> Camera)
	{
		C_P(PostProcessor);

		{
			ViewPort->SetRenderTarget();
			RHIContext.SetViewPort(0, 0, ViewPort->GetSize().x, ViewPort->GetSize().y);
			d->BloomEffect->Draw(RHIContext, TargetBuffer);
			ApplyBloom(RHIContext, TargetBuffer);
		}

		{
			ViewPort->SetRenderTarget();
			RHIContext.SetViewPort(0, 0, ViewPort->GetSize().x, ViewPort->GetSize().y);
			d->TAA->Draw(RHIContext, TargetBuffer, Camera);
			Tonemapping(RHIContext, TargetBuffer);
		}

	}

	void PostProcessor::Tonemapping(RenderCore::RHICommandContext& RHIContext, std::shared_ptr<GBuffer> TargetBuffer)
	{
		C_P(PostProcessor);
		RenderCore::RHICommandMark Mark(RHIContext, "Tonemapping");

		RenderCore::GraphicsPipelineStateInitializer Init;
		Init.VertexShader = d->VertexShader;
		Init.PixelShader = d->PixelShader;

		Init.BlendState = RenderCore::RHICachedStates::BlendTraditional;
		Init.DepthStencilState = RenderCore::RHICachedStates::DepthStateDisable;
		Init.RasterizerState = RenderCore::RHICachedStates::RasterizerStateCullNone;

		RHIContext.RHISetGraphicsPipelineState(Init);
		RHIContext.RHISetShaderSampler(RenderCore::SF_Pixel, 0, RenderCore::RHICachedStates::ClampLinerSampler);
		RHIContext.RHISetShaderTexture(RenderCore::SF_Pixel, 0, TargetBuffer->GetSceneColor());

		RHIContext.Draw(3);
	}

	void PostProcessor::ApplyBloom(RenderCore::RHICommandContext& RHIContext, std::shared_ptr<GBuffer> TargetBuffer)
	{
		C_P(PostProcessor);
		if (!d->BloomEffect->GetResult())
			return;
		RenderCore::RHICommandMark Mark(RHIContext, "ApplyBloom");
		RHIContext.SetRenderTarget(TargetBuffer->GetSceneColorWithBloom(), nullptr);
		RenderCore::GraphicsPipelineStateInitializer Init;
		Init.VertexShader = d->VertexShader;
		Init.PixelShader = d->AppalyBloomShader;

		Init.BlendState = RenderCore::RHICachedStates::BlendTraditional;
		Init.DepthStencilState = RenderCore::RHICachedStates::DepthStateDisable;
		Init.RasterizerState = RenderCore::RHICachedStates::RasterizerStateCullNone;

		RHIContext.RHISetGraphicsPipelineState(Init);
		RHIContext.RHISetShaderSampler(RenderCore::SF_Pixel, 0, RenderCore::RHICachedStates::ClampLinerSampler);
		RHIContext.RHISetShaderTexture(RenderCore::SF_Pixel, 0, TargetBuffer->GetSceneColor());
		RHIContext.RHISetShaderTexture(RenderCore::SF_Pixel, 1, d->BloomEffect->GetResult());
		d->GET_UNIFORMDATA(BloomContants).BloomIntensity = 1.0f;
		d->GET_SHADER_STRUCT_MEMBER(BloomContants).UpdateUniformBuffer();
		d->GET_SHADER_STRUCT_MEMBER(BloomContants).SetShaderUniformBuffer(RenderCore::EShaderFrequency::SF_Pixel);
		RHIContext.Draw(3);
	}

}