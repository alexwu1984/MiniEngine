#include "Render/PostProcessor.h"
#include "core/system.h"
#include "RHI/RHIShdader.h"
#include "RHI/RHIShaderDefine.h"
#include "RHI/RHIPipeLineState.h"
#include "RHI/RHICachedStates.h"
#include "RHI/DynamicRHI.h"
#include "Engine/Engine.h"
#include "Render/GBuffer.h"
#include "Render/TemporalAA.h"

namespace Engine
{
	using namespace RenderCore;
	struct PostProcessorPrivate
	{
		DynamicRHI* RHI = nullptr;
		std::shared_ptr< RHIVertexShader> VertexShader;
		std::shared_ptr< RHIPixelShader> PixelShader;
		std::shared_ptr< TemporallAA> TAA;
	};

	PostProcessor::PostProcessor(RenderCore::DynamicRHI* RHI)
		:d_ptr(new PostProcessorPrivate())
	{
		C_P(PostProcessor);
		d->RHI = RHI;
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

		d->TAA = std::make_shared<TemporallAA>(d->RHI);
		d->TAA->InitResource();
	}

	void PostProcessor::Draw(RenderCore::RHICommandContext& RHIContext, std::shared_ptr<GBuffer> TargetBuffer)
	{
		C_P(PostProcessor);
		d->TAA->Draw(RHIContext, TargetBuffer);
		Tonemapping(RHIContext, TargetBuffer);
	}

	void PostProcessor::Tonemapping(RenderCore::RHICommandContext& RHIContext, std::shared_ptr<GBuffer> TargetBuffer)
	{
		C_P(PostProcessor);
		GraphicsPipelineStateInitializer Init;
		Init.VertexShader = d->VertexShader;
		Init.PixelShader = d->PixelShader;

		Init.BlendState = RHICachedStates::BlendOnAlphaOff;
		Init.DepthStencilState = RHICachedStates::DepthStateDisable;
		Init.RasterizerState = RHICachedStates::RasterizerStateCullNone;

		RHIContext.RHISetGraphicsPipelineState(Init);
		RHIContext.RHISetShaderSampler(RenderCore::SF_Pixel, 0, RHICachedStates::ClampLinerSampler);
		RHIContext.RHISetShaderTexture(RenderCore::SF_Pixel, 0, TargetBuffer->GetSceneColor());
		RHIContext.Draw(3);
	}

}