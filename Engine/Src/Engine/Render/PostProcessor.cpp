#include "Render/PostProcessor.h"
#include "core/system.h"
#include <cmath>
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
#include "Render/FrameGraph.h"
#include "Render/RenderTexturePool.h"
#include "Render/PostProcessPass.h"
#include "Render/MaterialPreFrame.h"
#include "Render/SceneRendering/FSceneViewData.h"
#include "Render/SceneRender.h"
#include "Engine/Render/PreProcessor.h"
#include "Engine/Render/IBLRender.h"

namespace Engine
{
	using namespace RenderCore;

	namespace
	{
		void ApplyRDGCompileParamsFromJson(const nlohmann::json& Root, FrameGraphCompileParams& Out)
		{
			try
			{
				if (Root.find("RDG") == Root.end() || !Root["RDG"].is_object())
					return;
				const auto& J = Root["RDG"];
				Out.bPassCullingFromSinks = J.value("PassCullingFromSinks", Out.bPassCullingFromSinks);
				Out.bDumpDotToLog = J.value("DumpDotToLog", Out.bDumpDotToLog);
				Out.bLogCompileSummary = J.value("LogCompileSummary", Out.bLogCompileSummary);
				Out.bLogRenderTexturePoolStats = J.value("LogRenderTexturePoolStats", Out.bLogRenderTexturePoolStats);
			}
			catch (const std::exception&)
			{
			}
		}
	} // namespace

	static void RegisterPostOnlyGBufferImports(FrameGraph& Graph, std::shared_ptr<GBuffer> TB)
	{
		if (!TB)
			return;
		Graph.ImportTexture("SceneColor", [TB]() { return TB->GetSceneColor(); }, false);
		Graph.ImportTexture("MotionVector", [TB]() { return TB->GetMotionVector(); }, false);
		Graph.ImportTexture("Normal", [TB]() { return TB->GetNormalBuffer(); }, false);
		Graph.ImportTexture("MetallicRoughness", [TB]() { return TB->GetMetallicRoughnessBuffer(); }, false);
		Graph.ImportTexture("Depth", [TB]() { return TB->GetDepth(); }, false);
	}

	struct PostProcessorPrivate
	{
		DynamicRHI* RHI = nullptr;
		std::shared_ptr<PostProcessFullscreenShaders> FullscreenShaders;
		std::shared_ptr<TonemappingPass> Tonemapping;
		std::shared_ptr<ApplyBloomPass> ApplyBloom;
		std::shared_ptr<ApplySSRPass> ApplySSR;
		std::shared_ptr<TemporallAA> TAA;
		std::shared_ptr<RenderCore::FXAA> FXaa;
		std::shared_ptr<Bloom> BloomEffect;
		std::shared_ptr<SSRProcessor> SSREffect;
		bool EnableSSR = false;
		EPostProcessorAAType AAType = EPostProcessorAAType::TAA;
		bool IsResourceInitialized = false;
		FrameGraphCompileParams RDGCompileParams{};
		/** EV-style exposure before tonemap / bloom threshold scaling (see Evn.ExposureStops in scene JSON). */
		float ExposureStops = 0.f;

		PostProcessorPrivate(DynamicRHI* _RHI) :
			GET_SHADER_STRUCT_MEMBER(BloomContants)(_RHI)
			, RHI(_RHI)
		{

		}
		DECLARE_SHADER_STRUCT_MEMBER(BloomContants);
	};

	struct PostProcessPassInputs
	{
		// SSR samples this texture at the reflected hit point.
		// TAA can provide stable history; FXAA uses current scene color to avoid SSR/FXAA feedback.
		std::shared_ptr<RenderCore::RHITexture2D> SSRReflectionColor;

		// Anti-aliasing runs after SSR and bloom have been composed into SceneColorWithBloom.
		std::shared_ptr<RenderCore::RHITexture2D> AntiAliasingColor;
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

	void PostProcessor::LoadConfig(const nlohmann::json& Root)
	{
		try
		{
			C_P(PostProcessor);
			ApplyRDGCompileParamsFromJson(Root, d->RDGCompileParams);
			RenderTexturePool::Get().ApplyConfigFromJson(Root);
			nlohmann::json EvnJson = Root["Evn"];
			d->EnableSSR = EvnJson.value("EnableSSR", false);
			d->ExposureStops = EvnJson.value("ExposureStops", 0.f);

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
		if (!d->FullscreenShaders)
			d->FullscreenShaders = std::make_shared<PostProcessFullscreenShaders>(d->RHI);
		d->FullscreenShaders->InitResource();
		std::shared_ptr<RHIVertexShader> FullscreenVertexShader = d->FullscreenShaders->GetVertexShader();

		d->Tonemapping = std::make_shared<TonemappingPass>(d->RHI, FullscreenVertexShader, &d->GET_SHADER_STRUCT_MEMBER(BloomContants));
		d->Tonemapping->InitResource();

		d->ApplyBloom = std::make_shared<ApplyBloomPass>(
			d->RHI,
			FullscreenVertexShader,
			&d->GET_SHADER_STRUCT_MEMBER(BloomContants));
		d->ApplyBloom->InitResource();

		d->ApplySSR = std::make_shared<ApplySSRPass>(d->RHI, FullscreenVertexShader);
		d->ApplySSR->InitResource();

		d->TAA.reset();
		d->FXaa.reset();
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

	void PostProcessor::InvalidateTransientResources()
	{
		C_P(PostProcessor);
		if (d->BloomEffect)
			d->BloomEffect->InvalidateTransientResources();
		if (d->SSREffect)
			d->SSREffect->InvalidateTransientResources();
		if (d->TAA)
			d->TAA->InvalidateTransientResources();
		if (d->FXaa)
			d->FXaa->InvalidateTransientResources();
	}

	void PostProcessor::Draw(RHICommandContext& RHIContext, std::shared_ptr<GBuffer> TargetBuffer, std::shared_ptr<RHIViewPort> ViewPort,
						   std::shared_ptr<const FSceneViewData> ViewData)
	{
		C_P(PostProcessor);
		FrameGraph Graph;
		RegisterPostOnlyGBufferImports(Graph, TargetBuffer);
		AddFramePasses(Graph, RHIContext, TargetBuffer, ViewPort, ViewData);
		Graph.Execute(d->RDGCompileParams);
	}

	void PostProcessor::AddFramePasses(FrameGraph& Graph, RHICommandContext& RHIContext, std::shared_ptr<GBuffer> TargetBuffer,
									   std::shared_ptr<RHIViewPort> ViewPort, std::shared_ptr<const FSceneViewData> ViewData)
	{
		C_P(PostProcessor);
		if (!ViewData)
			return;

		{
			auto& BloomData = d->GET_UNIFORMDATA(BloomContants);
			BloomData.PostExposureLinear = powf(2.f, d->ExposureStops);
			d->GET_SHADER_STRUCT_MEMBER(BloomContants).UpdateUniformBuffer();
		}

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

		if (PassInputs.SSRReflectionColor)
		{
			std::shared_ptr<RHITexture2D> ReflectionSnap = PassInputs.SSRReflectionColor;
			Graph.ImportTexture("ReflectionColor", [ReflectionSnap]() { return ReflectionSnap; }, false);
		}

		const bool UseSSRComposite = d->EnableSSR && d->SSREffect && PassInputs.SSRReflectionColor;
		BuildSSRPasses(Graph, RHIContext, TargetBuffer, ViewPort, ViewData, PassInputs.SSRReflectionColor);
		BuildBloomPasses(Graph, RHIContext, TargetBuffer, ViewPort, UseSSRComposite);
		BuildAAPasses(Graph, RHIContext, TargetBuffer, ViewPort, ViewData, PassInputs.AntiAliasingColor);
		BuildTonemappingPass(Graph, RHIContext, TargetBuffer, ViewPort);
	}

	void PostProcessor::BuildSSRPasses(FrameGraph& Graph, RenderCore::RHICommandContext& RHIContext, std::shared_ptr<GBuffer> TargetBuffer,
									   std::shared_ptr<RenderCore::RHIViewPort> ViewPort, std::shared_ptr<const FSceneViewData> ViewData,
									   std::shared_ptr<RenderCore::RHITexture2D> SSRReflectionColor)
	{
		C_P(PostProcessor);
		if (!d->EnableSSR || !d->SSREffect || !SSRReflectionColor)
			return;

		SSRPass SSRPassNode(
			RHIContext,
			TargetBuffer,
			ViewPort,
			ViewData,
			d->SSREffect,
			[SSRReflectionColor]() { return SSRReflectionColor; });
		Graph.AddPass(SSRPassNode.BuildDesc());

		Graph.AddPass(d->ApplySSR->BuildDesc(
			RHIContext,
			TargetBuffer,
			ViewPort,
			[d]() { return d->SSREffect ? d->SSREffect->GetSSRBuffer() : std::shared_ptr<RenderCore::RHITexture2D>{}; }));
	}

	void PostProcessor::BuildBloomPasses(FrameGraph& Graph, RenderCore::RHICommandContext& RHIContext, std::shared_ptr<GBuffer> TargetBuffer,
										 std::shared_ptr<RenderCore::RHIViewPort> ViewPort, bool UseSSRComposite)
	{
		C_P(PostProcessor);
		const std::string bloomSceneInput = UseSSRComposite ? "SceneColorWithSSR" : "SceneColor";
		BloomPass BloomPassNode(
			RHIContext,
			TargetBuffer,
			ViewPort,
			d->BloomEffect,
			[TargetBuffer, UseSSRComposite]() { return UseSSRComposite ? TargetBuffer->GetSceneColorWithSSR() : TargetBuffer->GetSceneColor(); },
			bloomSceneInput);
		Graph.AddPass(BloomPassNode.BuildDesc());

		Graph.AddPass(d->ApplyBloom->BuildDesc(
			RHIContext,
			TargetBuffer,
			ViewPort,
			bloomSceneInput,
			[TargetBuffer, UseSSRComposite]() { return UseSSRComposite ? TargetBuffer->GetSceneColorWithSSR() : TargetBuffer->GetSceneColor(); },
			[d]() { return d->BloomEffect ? d->BloomEffect->GetResult() : std::shared_ptr<RenderCore::RHITexture2D>{}; }));
	}

	void PostProcessor::BuildAAPasses(FrameGraph& Graph, RenderCore::RHICommandContext& RHIContext, std::shared_ptr<GBuffer> TargetBuffer,
									  std::shared_ptr<RenderCore::RHIViewPort> ViewPort, std::shared_ptr<const FSceneViewData> ViewData,
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
				ViewData,
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

	void PostProcessor::BuildTonemappingPass(FrameGraph& Graph, RenderCore::RHICommandContext& RHIContext, std::shared_ptr<GBuffer> TargetBuffer,
											 std::shared_ptr<RenderCore::RHIViewPort> ViewPort)
	{
		C_P(PostProcessor);
		const std::string tonemapInput = (d->AAType == EPostProcessorAAType::FXAA && d->FXaa) ? "FXAAResult" : "SceneColor";
		Graph.AddPass(d->Tonemapping->BuildDesc(
			RHIContext,
			TargetBuffer,
			ViewPort,
			[d, TargetBuffer]() { return d->AAType == EPostProcessorAAType::FXAA && d->FXaa ? d->FXaa->GetResult() : TargetBuffer->GetSceneColor(); },
			tonemapInput));
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

	bool PostProcessor::WantsHaltonProjectionJitterForMainPass() const
	{
		C_P(PostProcessor);
		return d->AAType == EPostProcessorAAType::TAA;
	}

}