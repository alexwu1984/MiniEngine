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
#include "Render/PostProcessGraph.h"
#include "Render/PostProcessPass.h"
#include "Render/MaterialPreFrame.h"
#include "Scene/CameraComponent.h"
#include "Render/SceneRender.h"
#include "Engine/Render/PreProcessor.h"
#include "Engine/Render/IBLRender.h"

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
		EPostProcessorAAType AAType = EPostProcessorAAType::TAA;
		bool IsResourceInitialized = false;

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

	struct PostProcessPassInputs
	{
		// SSR samples this texture at the reflected hit point.
		// TAA can provide stable history; FXAA uses current scene color to avoid SSR/FXAA feedback.
		std::shared_ptr<RenderCore::RHITexture2D> SSRReflectionColor;

		// Anti-aliasing runs after SSR and bloom have been composed into SceneColorWithBloom.
		std::shared_ptr<RenderCore::RHITexture2D> AntiAliasingColor;
	};

	FullscreenPostProcessPassResources GetFullscreenPassResources(PostProcessorPrivate* PrivateData)
	{
		FullscreenPostProcessPassResources Resources;
		Resources.VertexShader = PrivateData->VertexShader;
		Resources.TonemappingShader = PrivateData->PixelShader;
		Resources.ApplyBloomShader = PrivateData->AppalyBloomShader;
		Resources.ApplySSRShader = PrivateData->AppalySSRShader;
		Resources.BloomConstants = &PrivateData->GET_SHADER_STRUCT_MEMBER(BloomContants);
		return Resources;
	}

	PostProcessor::PostProcessor(RenderCore::DynamicRHI* RHI)
		:d_ptr(new PostProcessorPrivate(RHI))
	{
		C_P(PostProcessor);
	}

	PostProcessor::~PostProcessor()
	{
		delete d_ptr;
	}

	void PostProcessor::LoadConfig(const nlohmann::json& Root)
	{
		try
		{
			C_P(PostProcessor);
			nlohmann::json EvnJson = Root["Evn"];
			d->EnableSSR = EvnJson.value("EnableSSR", false);

			EPostProcessorAAType ConfigAAType = d->AAType;
			if (EvnJson.find("AAType") != EvnJson.end())
			{
				const auto& AATypeJson = EvnJson["AAType"];
				if (AATypeJson.is_string())
				{
					const std::string AAType = AATypeJson.get<std::string>();
					if (AAType == "FXAA" || AAType == "fxaa")
						ConfigAAType = EPostProcessorAAType::FXAA;
					else if (AAType == "TAA" || AAType == "taa")
						ConfigAAType = EPostProcessorAAType::TAA;
				}
				else if (AATypeJson.is_number_integer())
				{
					ConfigAAType = AATypeJson.get<int>() == 1 ? EPostProcessorAAType::FXAA : EPostProcessorAAType::TAA;
				}
			}

			if (d->AAType != ConfigAAType)
			{
				d->AAType = ConfigAAType;
				d->TAA.reset();
				d->FXaa.reset();
				if (d->IsResourceInitialized)
					InitResource();
			}
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
		d->IsResourceInitialized = true;
	}

	void PostProcessor::Draw(RHICommandContext& RHIContext, std::shared_ptr<GBuffer> TargetBuffer, 
						     std::shared_ptr<RHIViewPort> ViewPort, std::shared_ptr<CameraComponent> Camera)
	{
		C_P(PostProcessor);

		PostProcessPassInputs PassInputs;
		PassInputs.AntiAliasingColor = TargetBuffer->GetSceneColorWithBloom();
		switch (d->AAType)
		{
		case EPostProcessorAAType::TAA:
			PassInputs.SSRReflectionColor = d->TAA->GetHistoryBuffer();
			if (!PassInputs.SSRReflectionColor)
				PassInputs.SSRReflectionColor = TargetBuffer->GetSceneColor();
			break;
		case EPostProcessorAAType::FXAA:
			PassInputs.SSRReflectionColor = TargetBuffer->GetSceneColor();
			break;
		}

		PostProcessGraph Graph;
		const bool UseSSRComposite = d->EnableSSR && d->SSREffect && PassInputs.SSRReflectionColor;
		BuildSSRPasses(Graph, RHIContext, TargetBuffer, ViewPort, Camera, PassInputs.SSRReflectionColor);
		BuildBloomPasses(Graph, RHIContext, TargetBuffer, ViewPort, UseSSRComposite);
		BuildAAPasses(Graph, RHIContext, TargetBuffer, ViewPort, Camera, PassInputs.AntiAliasingColor);
		BuildTonemappingPass(Graph, RHIContext, TargetBuffer, ViewPort);
		Graph.Execute();
	}

	void PostProcessor::BuildSSRPasses(PostProcessGraph& Graph, RenderCore::RHICommandContext& RHIContext, std::shared_ptr<GBuffer> TargetBuffer,
									   std::shared_ptr<RenderCore::RHIViewPort> ViewPort, std::shared_ptr<CameraComponent> Camera,
									   std::shared_ptr<RenderCore::RHITexture2D> SSRReflectionColor)
	{
		C_P(PostProcessor);
		if (!d->EnableSSR || !d->SSREffect || !SSRReflectionColor)
			return;

		Graph.AddPass({
			"SSR",
			{
				{ "Normal", [TargetBuffer]() { return TargetBuffer->GetNormalBuffer(); } },
				{ "MetallicRoughness", [TargetBuffer]() { return TargetBuffer->GetMetallicRoughnessBuffer(); } },
				{ "Depth", [TargetBuffer]() { return TargetBuffer->GetDepth(); } },
				{ "ReflectionColor", [SSRReflectionColor]() { return SSRReflectionColor; } }
			},
			{
				{ "SSRBuffer", [d]() { return d->SSREffect ? d->SSREffect->GetSSRBuffer() : std::shared_ptr<RenderCore::RHITexture2D>{}; }, false }
			},
			[this, d, &RHIContext, TargetBuffer, ViewPort, Camera, SSRReflectionColor]() {
				d->SSREffect->Draw(RHIContext, TargetBuffer, ViewPort, SSRReflectionColor, Camera);
			}
		});

		ApplySSRPass Pass(
			RHIContext,
			TargetBuffer,
			ViewPort,
			GetFullscreenPassResources(d),
			[d]() { return d->SSREffect ? d->SSREffect->GetSSRBuffer() : std::shared_ptr<RenderCore::RHITexture2D>{}; });
		Graph.AddPass(Pass.BuildDesc());
	}

	void PostProcessor::BuildBloomPasses(PostProcessGraph& Graph, RenderCore::RHICommandContext& RHIContext, std::shared_ptr<GBuffer> TargetBuffer,
										 std::shared_ptr<RenderCore::RHIViewPort> ViewPort, bool UseSSRComposite)
	{
		C_P(PostProcessor);
		Graph.AddPass({
			"Bloom",
			{
				{ "SourceColor", [TargetBuffer, UseSSRComposite]() { return UseSSRComposite ? TargetBuffer->GetSceneColorWithSSR() : TargetBuffer->GetSceneColor(); } }
			},
			{
				{ "BloomResult", [d]() { return d->BloomEffect ? d->BloomEffect->GetResult() : std::shared_ptr<RenderCore::RHITexture2D>{}; }, false }
			},
			[d, &RHIContext, TargetBuffer, ViewPort]() {
				ViewPort->SetRenderTarget();
				RHIContext.SetViewPort(0, 0, ViewPort->GetSize().x, ViewPort->GetSize().y);
				d->BloomEffect->Draw(RHIContext, TargetBuffer);
			}
		});

		ApplyBloomPass Pass(
			RHIContext,
			TargetBuffer,
			ViewPort,
			GetFullscreenPassResources(d),
			[TargetBuffer, UseSSRComposite]() { return UseSSRComposite ? TargetBuffer->GetSceneColorWithSSR() : TargetBuffer->GetSceneColor(); },
			[d]() { return d->BloomEffect ? d->BloomEffect->GetResult() : std::shared_ptr<RenderCore::RHITexture2D>{}; });
		Graph.AddPass(Pass.BuildDesc());
	}

	void PostProcessor::BuildAAPasses(PostProcessGraph& Graph, RenderCore::RHICommandContext& RHIContext, std::shared_ptr<GBuffer> TargetBuffer,
									  std::shared_ptr<RenderCore::RHIViewPort> ViewPort, std::shared_ptr<CameraComponent> Camera,
									  std::shared_ptr<RenderCore::RHITexture2D> AntiAliasingColor)
	{
		C_P(PostProcessor);
		switch (d->AAType)
		{
		case EPostProcessorAAType::TAA:
		{
			TAAPass Pass(
				RHIContext,
				TargetBuffer,
				ViewPort,
				Camera,
				d->TAA,
				[AntiAliasingColor]() { return AntiAliasingColor; });
			Graph.AddPass(Pass.BuildDesc());
			break;
		}
		case EPostProcessorAAType::FXAA:
		{
			FXAAPass Pass(
				RHIContext,
				ViewPort,
				d->FXaa,
				[AntiAliasingColor]() { return AntiAliasingColor; });
			Graph.AddPass(Pass.BuildDesc());
			break;
		}
		}
	}

	void PostProcessor::BuildTonemappingPass(PostProcessGraph& Graph, RenderCore::RHICommandContext& RHIContext, std::shared_ptr<GBuffer> TargetBuffer,
											 std::shared_ptr<RenderCore::RHIViewPort> ViewPort)
	{
		C_P(PostProcessor);
		TonemappingPass Pass(
			RHIContext,
			TargetBuffer,
			ViewPort,
			GetFullscreenPassResources(d),
			[d, TargetBuffer]() { return d->AAType == EPostProcessorAAType::FXAA && d->FXaa ? d->FXaa->GetResult() : TargetBuffer->GetSceneColor(); });
		Graph.AddPass(Pass.BuildDesc());
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

}