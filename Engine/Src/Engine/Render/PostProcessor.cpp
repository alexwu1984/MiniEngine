#include "Render/PostProcessor.h"
#include "core/system.h"
#include "RHI/RHIShdader.h"
#include "RHI/RHIShaderDefine.h"
#include "RHI/RHIPipeLineState.h"
#include "RHI/RHICachedStates.h"
#include "RHI/DynamicRHI.h"
#include "RHI/RHIViewPort.h"
#include "RHI/RHITextureCube.h"
#include "Engine/Engine.h"
#include "Render/GBuffer.h"
#include "Render/TemporalAA.h"
#include "Render/FXAA.h"
#include "Render/Bloom.h"
#include "Render/RenderUtil.h"
#include "Render/SSRProcessor.h"
#include "Render/MaterialPreFrame.h"
#include "Scene/CameraComponent.h"
#include "Render/SceneRender.h"
#include "Engine/Render/PreProcessor.h"
#include "Engine/Render/IBLRender.h"
#include "tinygltf/json.h"

namespace Engine
{
	using namespace RenderCore;
	struct PostProcessorPrivate
	{
		DynamicRHI* RHI = nullptr;
		std::shared_ptr<RHIVertexShader> VertexShader;
		std::shared_ptr<RHIPixelShader> PixelShader;
		std::shared_ptr<RHIPixelShader> AppalyBloomShader;
		std::shared_ptr<RHIPixelShader> AppalySSRShader;
		std::shared_ptr<TemporallAA> TAA;
		std::shared_ptr<RenderCore::FXAA> FXaa;
		std::shared_ptr<Bloom> BloomEffect;
		std::shared_ptr<SSRProcessor> SSREffect;
		bool EnableSSR = false;
		EPostProcessorAAType AAType = EPostProcessorAAType::FXAA;

		PostProcessorPrivate(DynamicRHI* _RHI) :
			GET_SHADER_STRUCT_MEMBER(BloomContants)(_RHI)
			, RHI(_RHI)
			, GET_SHADER_STRUCT_MEMBER(CBPerFrame)(_RHI)
			, GET_SHADER_STRUCT_MEMBER(ENVContant)(_RHI)
		{

		}
		DECLARE_SHADER_STRUCT_MEMBER(BloomContants);
		DECLARE_SHADER_STRUCT_MEMBER(CBPerFrame);
		DECLARE_SHADER_STRUCT_MEMBER(ENVContant);
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

	void PostProcessor::LoadConfig(const std::wstring& FileName)
	{
		try
		{
			C_P(PostProcessor);
			nlohmann::json Root;
			std::ifstream input_json_file(FileName);
			if (!input_json_file.is_open())
			{
				return;
			}

			input_json_file >> Root;
			nlohmann::json EvnJson = Root["Evn"];
			d->EnableSSR = EvnJson.value("EnableSSR", false);
		}
		catch (const std::exception&)
		{

		}
	}

	void PostProcessor::InitResource()
	{
		C_P(PostProcessor);
		std::wstring ShaderPath = core::process_directory().wstring() + L"/ShaderLibDX/";
		ShaderPath += L"PostProcess.hlsl";

		d->VertexShader = d->RHI->RHICreateVertexShader(ShaderPath, "VS_ScreenQuad", {}, {});
		d->PixelShader = d->RHI->RHICreatePixelShader(ShaderPath, "PS_Tonemapping", {});
		d->AppalyBloomShader = d->RHI->RHICreatePixelShader(ShaderPath, "PS_ApplyBloom", {});
		d->AppalySSRShader = d->RHI->RHICreatePixelShader(ShaderPath, "PS_ApplySSR", {});

		switch (d->AAType)
		{
		case EPostProcessorAAType::TAA:
			d->TAA = std::make_shared<TemporallAA>(d->RHI);
			d->TAA->InitResource();
			break;
		case EPostProcessorAAType::FXAA:
			d->FXaa = std::make_shared<FXAA>(d->RHI);
			d->FXaa->InitResource();
			break;
		}

		d->BloomEffect = std::make_shared<Bloom>(d->RHI);
		d->BloomEffect->InitResource();

		d->SSREffect = std::make_shared<SSRProcessor>(d->RHI);
		d->SSREffect->InitResource();
	}

	void PostProcessor::Draw(RHICommandContext& RHIContext, std::shared_ptr<GBuffer> TargetBuffer, 
						     std::shared_ptr<RHIViewPort> ViewPort, std::shared_ptr<CameraComponent> Camera)
	{
		C_P(PostProcessor);

		std::shared_ptr<RHITexture2D> AABuffer;
		switch (d->AAType)
		{
		case EPostProcessorAAType::TAA:
			AABuffer = d->TAA->GetHistoryBuffer();
			break;
		case EPostProcessorAAType::FXAA:
			AABuffer = d->FXaa->GetResult();
			break;
		}

		if (d->EnableSSR && d->SSREffect && AABuffer)
			d->SSREffect->Draw(RHIContext, TargetBuffer, ViewPort, AABuffer, Camera);

		{
			ViewPort->SetRenderTarget();
			RHIContext.SetViewPort(0, 0, ViewPort->GetSize().x, ViewPort->GetSize().y);
			if (d->SSREffect && d->SSREffect->GetSSRBuffer())
				ApplySSR(RHIContext, TargetBuffer);
			d->BloomEffect->Draw(RHIContext, TargetBuffer);
			ApplyBloom(RHIContext, TargetBuffer);
		}

		{
			ViewPort->SetRenderTarget();
			RHIContext.SetViewPort(0, 0, ViewPort->GetSize().x, ViewPort->GetSize().y);

			switch (d->AAType)
			{
			case EPostProcessorAAType::TAA:
				d->TAA->Draw(RHIContext, TargetBuffer, Camera);
				break;
			case EPostProcessorAAType::FXAA:
				d->FXaa->Draw(RHIContext, TargetBuffer->GetSceneColorWithBloom());
				break;
			}

			Tonemapping(RHIContext, TargetBuffer, ViewPort);
		}

	}

	std::shared_ptr<RenderCore::RHITexture2D> PostProcessor::GetSSRBuffer() const
	{
		C_P(PostProcessor);
		if (d->SSREffect)
			return d->SSREffect->GetSSRBuffer();
		return {};
	}

	Engine::EPostProcessorAAType PostProcessor::GetPostProcessorAAType() const
	{
		C_P(PostProcessor);
		return d->AAType;
	}

	void PostProcessor::Tonemapping(RenderCore::RHICommandContext& RHIContext, std::shared_ptr<GBuffer> TargetBuffer,
									std::shared_ptr<RenderCore::RHIViewPort> ViewPort)
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
		ViewPort->SetRenderTarget();
		RHIContext.SetViewPort(0, 0, ViewPort->GetSize().x, ViewPort->GetSize().y);
		RHIContext.RHISetShaderSampler(RenderCore::SF_Pixel, 0, RenderCore::RHICachedStates::ClampLinerSampler);
		
		// Use FXAA result if FXAA is enabled, otherwise use original scene color
		std::shared_ptr<RenderCore::RHITexture2D> SourceTexture = TargetBuffer->GetSceneColor();
		if (d->AAType == EPostProcessorAAType::FXAA && d->FXaa)
		{
			SourceTexture = d->FXaa->GetResult();
		}
		RHIContext.RHISetShaderTexture(RenderCore::SF_Pixel, 0, SourceTexture);

		RHIContext.Draw(3);
	}

	void PostProcessor::ApplyBloom(RenderCore::RHICommandContext& RHIContext, std::shared_ptr<GBuffer> TargetBuffer)
	{
		C_P(PostProcessor);
		if (!d->BloomEffect->GetResult())
			return;
		RenderCore::RHICommandMark Mark(RHIContext, "ApplyBloom");
		RenderCore::GraphicsPipelineStateInitializer Init;
		Init.VertexShader = d->VertexShader;
		Init.PixelShader = d->AppalyBloomShader;

		Init.BlendState = RenderCore::RHICachedStates::BlendTraditional;
		Init.DepthStencilState = RenderCore::RHICachedStates::DepthStateDisable;
		Init.RasterizerState = RenderCore::RHICachedStates::RasterizerStateCullNone;

		RHIContext.RHISetGraphicsPipelineState(Init);
		RHIContext.SetRenderTarget(TargetBuffer->GetSceneColorWithBloom(), nullptr);
		RHIContext.RHISetShaderSampler(RenderCore::SF_Pixel, 0, RenderCore::RHICachedStates::ClampLinerSampler);
		RHIContext.RHISetShaderTexture(RenderCore::SF_Pixel, 0, d->EnableSSR ? TargetBuffer->GetSceneColorWithSSR() : TargetBuffer->GetSceneColor());
		RHIContext.RHISetShaderTexture(RenderCore::SF_Pixel, 1, d->BloomEffect->GetResult());
		d->GET_UNIFORMDATA(BloomContants).BloomIntensity = 1.0f;
		d->GET_SHADER_STRUCT_MEMBER(BloomContants).UpdateUniformBuffer();
		d->GET_SHADER_STRUCT_MEMBER(BloomContants).SetShaderUniformBuffer(RenderCore::EShaderFrequency::SF_Pixel);
		RHIContext.Draw(3);
	}

	void PostProcessor::ApplySSR(RenderCore::RHICommandContext& RHIContext, std::shared_ptr<GBuffer> TargetBuffer)
	{
		C_P(PostProcessor);
		RenderCore::RHICommandMark Mark(RHIContext, "ApplySSR");
		RenderCore::GraphicsPipelineStateInitializer Init;
		Init.VertexShader = d->VertexShader;
		Init.PixelShader = d->AppalySSRShader;

		Init.BlendState = RenderCore::RHICachedStates::BlendTraditional;
		Init.DepthStencilState = RenderCore::RHICachedStates::DepthStateDisable;
		Init.RasterizerState = RenderCore::RHICachedStates::RasterizerStateCullNone;

		RHIContext.RHISetGraphicsPipelineState(Init);
		RHIContext.SetRenderTarget(TargetBuffer->GetSceneColorWithSSR(), nullptr);
		RHIContext.RHISetShaderSampler(RenderCore::SF_Pixel, 0, RenderCore::RHICachedStates::ClampLinerSampler);
		RHIContext.RHISetShaderTexture(RenderCore::SF_Pixel, 0, TargetBuffer->GetSceneColor());
		RHIContext.RHISetShaderTexture(RenderCore::SF_Pixel, 1, d->SSREffect->GetSSRBuffer());
		RHIContext.Draw(3);
	}

}