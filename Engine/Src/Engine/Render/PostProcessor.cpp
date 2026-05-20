#include "Render/PostProcessor.h"
#include "core/logger.h"
#include "core/system.h"
#include "RHI/RHIShdader.h"
#include "RHI/RHIShaderDefine.h"
#include "RHI/RHIPipeLineState.h"
#include "RHI/RHICachedStates.h"
#include "RHI/DynamicRHI.h"
#include "RHI/RHIViewPort.h"
#include "RHI/RHITextureCube.h"
#include "Engine/Engine.h"
#include "Render/SceneTextures.h"
#include "Render/TemporalAA.h"
#include "Render/FXAA.h"
#include "Render/Bloom.h"
#include "Render/RenderUtil.h"
#include "Render/SSRProcessor.h"
#include "Render/RDGBuilder.h"
#include "Render/RenderTexturePool.h"
#include "Render/PostProcessPass.h"
#include "Render/MaterialPreFrame.h"
#include "Render/SceneRendering/SceneViewData.h"
#include "RHI/RHITexture2D.h"

namespace Engine
{
	using namespace RenderCore;

	namespace
	{
		void ApplyRDGCompileParamsFromJson(const nlohmann::json& Root, FRDGCompileParameters& Out)
		{
			try
			{
				if (Root.find("RDG") == Root.end() || !Root["RDG"].is_object())
					return;
				const auto& J = Root["RDG"];
				Out.bPassCullingFromSinks = J.value("PassCullingFromSinks", Out.bPassCullingFromSinks);
				Out.bLogCompileSummary = J.value("LogCompileSummary", Out.bLogCompileSummary);
				Out.bLogRenderTexturePoolStats = J.value("LogRenderTexturePoolStats", Out.bLogRenderTexturePoolStats);
				Out.bRDGAutoPipelineBarriers = J.value("AutoPipelineBarriers", Out.bRDGAutoPipelineBarriers);
			}
			catch (const std::exception&)
			{
			}
		}
	} // namespace

	static void RegisterBloomTransientUavs(FRDGBuilder& Graph, const std::shared_ptr<FSceneTextures>& SceneTextures)
	{
		if (!SceneTextures || !SceneTextures->GetSceneColor())
			return;

		auto MkDescForLevel = [SceneTextures](int Level)
		{
			FRDGTransientUAVDesc Desc;
			Desc.PixelFormat = EPixelFormat::PF_FloatRGB;
			core::vec2i Sz = SceneTextures->GetSceneColor()->GetSize();
			for (int L = 0; L < Level; ++L)
			{
				Sz.x = (std::max)(1, Sz.x >> 1);
				Sz.y = (std::max)(1, Sz.y >> 1);
			}
			Desc.Width = Sz.x;
			Desc.Height = Sz.y;
			return Desc;
		};

		for (int Idx = 0; Idx < 5; ++Idx)
			Graph.RegisterTransientUAV(std::string("Bloom.Chain") + std::to_string(Idx),
									   [MkDescForLevel, Idx]()
									   {
										   return MkDescForLevel(Idx);
									   });
	}

	static void RegisterPostOnlySceneTexturesImports(FRDGBuilder& Graph, std::shared_ptr<FSceneTextures> SceneTextures)
	{
		if (!SceneTextures)
			return;
		Graph.ImportTexture("SceneColor", [SceneTextures]() { return SceneTextures->GetSceneColor(); }, false);
		Graph.ImportTexture("MotionVector", [SceneTextures]() { return SceneTextures->GetMotionVector(); }, false);
		Graph.ImportTexture("Normal", [SceneTextures]() { return SceneTextures->GetNormalBuffer(); }, false);
		Graph.ImportTexture("MetallicRoughness", [SceneTextures]() { return SceneTextures->GetMetallicRoughnessBuffer(); }, false);
		Graph.ImportTexture("Depth", [SceneTextures]() { return SceneTextures->GetDepth(); }, false);
	}

	struct PostProcessorPrivate
	{
		DynamicRHI* RHI = nullptr;
		std::shared_ptr<PostProcessFullscreenShaders> FullscreenShaders;
		std::shared_ptr<TonemappingPass> Tonemapping;
		std::shared_ptr<ApplyBloomPass> ApplyBloom;
		std::shared_ptr<ApplySSRPass> ApplySSR;
		std::shared_ptr<TemporallAA> TAA;
		std::shared_ptr<FXAA> FXaa;
		std::shared_ptr<Bloom> BloomEffect;
		std::shared_ptr<SSRProcessor> SSREffect;
		bool EnableSSR = false;
		EPostProcessorAAType AAType = EPostProcessorAAType::TAA;
		bool IsResourceInitialized = false;
		FRDGCompileParameters RDGCompileParams{};
		/** EV-style exposure before tonemap / bloom threshold scaling (see Evn.ExposureStops in scene JSON). */
		float ExposureStops = 0.f;
		/** Scales blurred emissive stack blended in ApplyBloom (see Evn.BloomIntensity). */
		float BloomIntensity = 1.f;
		/** Bloom extract threshold in linear luminance units (see Evn.BloomThreshold). */
		float BloomThreshold = 0.72f;

		PostProcessorPrivate(DynamicRHI* _RHI) :
			GET_SHADER_STRUCT_MEMBER(BloomContants)(_RHI)
			, RHI(_RHI)
		{

		}
		DECLARE_SHADER_STRUCT_MEMBER(BloomContants);
	};

	struct PostProcessPassInputs
	{
		// SSR samples this texture at the reflected hit point (TAA history when AA is TAA).
		std::shared_ptr<RenderCore::RHITexture2D> SSRReflectionColor;

		// TAA: lit SceneColor (pre-bloom). FXAA: SceneColorWithBloom after bloom composite.
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
			d->BloomIntensity = EvnJson.value("BloomIntensity", 1.f);
			d->BloomThreshold = EvnJson.value("BloomThreshold", 0.72f);

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

	void PostProcessor::Draw(RHICommandContext& RHIContext, std::shared_ptr<FSceneTextures> SceneTextures, std::shared_ptr<RHIViewPort> ViewPort,
						   std::shared_ptr<const FSceneViewData> ViewData)
	{
		C_P(PostProcessor);
		FRDGBuilder Graph;
		RegisterPostOnlySceneTexturesImports(Graph, SceneTextures);
		AddFramePasses(Graph, RHIContext, SceneTextures, ViewPort, ViewData);
		FRDGCompileParameters RDGExecParams = d->RDGCompileParams;
		RDGExecParams.RDGBarrierCommandContext = &RHIContext;
		RDGExecParams.RDGAcquirePooledResourcesRHI = d->RHI;
		if (!Graph.Compile(d->RDGCompileParams, nullptr))
		{
			core::LOG(core::log_err, L"FRDG: post-process graph compile failed (cycle); executing passes in AddPass order.");
			Graph.ExecutePassesInSetupOrder(RDGExecParams);
		}
		else
			Graph.ExecutePasses(RDGExecParams);
	}

	void PostProcessor::AddFramePasses(FRDGBuilder& Graph, RHICommandContext& RHIContext, std::shared_ptr<FSceneTextures> SceneTextures,
									   std::shared_ptr<RHIViewPort> ViewPort, std::shared_ptr<const FSceneViewData> ViewData)
	{
		C_P(PostProcessor);
		if (!ViewData)
			return;

		{
			auto& BloomData = d->GET_UNIFORMDATA(BloomContants);
			BloomData.PostExposureLinear = powf(2.f, d->ExposureStops);
			BloomData.BloomIntensity = d->BloomIntensity;
			BloomData.BloomThreshold = d->BloomThreshold;
			d->GET_SHADER_STRUCT_MEMBER(BloomContants).UpdateUniformBuffer(RHIContext);
		}

		PostProcessPassInputs PassInputs;
		switch (d->AAType)
		{
		case EPostProcessorAAType::TAA:
			PassInputs.AntiAliasingColor = SceneTextures->GetSceneColor();
			PassInputs.SSRReflectionColor = d->TAA->GetHistoryBuffer();
			if (!PassInputs.SSRReflectionColor)
				PassInputs.SSRReflectionColor = SceneTextures->GetSceneColor();
			break;
		case EPostProcessorAAType::FXAA:
			PassInputs.AntiAliasingColor = SceneTextures->GetSceneColorWithBloom();
			PassInputs.SSRReflectionColor = SceneTextures->GetSceneColor();
			break;
		}

		if (PassInputs.SSRReflectionColor)
		{
			std::shared_ptr<RHITexture2D> ReflectionSnap = PassInputs.SSRReflectionColor;
			Graph.ImportTexture("ReflectionColor", [ReflectionSnap]() { return ReflectionSnap; }, false);
		}

		const bool UseSSRComposite = d->EnableSSR && d->SSREffect && PassInputs.SSRReflectionColor;
		if (d->AAType == EPostProcessorAAType::TAA)
			BuildAAPasses(Graph, RHIContext, SceneTextures, ViewPort, ViewData, PassInputs.AntiAliasingColor);
		BuildSSRPasses(Graph, RHIContext, SceneTextures, ViewPort, ViewData, PassInputs.SSRReflectionColor);
		BuildBloomPasses(Graph, RHIContext, SceneTextures, ViewPort, UseSSRComposite);
		if (d->AAType == EPostProcessorAAType::FXAA)
			BuildAAPasses(Graph, RHIContext, SceneTextures, ViewPort, ViewData, PassInputs.AntiAliasingColor);
		BuildTonemappingPass(Graph, RHIContext, SceneTextures, ViewPort);
	}

	void PostProcessor::BuildSSRPasses(FRDGBuilder& Graph, RenderCore::RHICommandContext& RHIContext, std::shared_ptr<FSceneTextures> SceneTextures,
									   std::shared_ptr<RenderCore::RHIViewPort> ViewPort, std::shared_ptr<const FSceneViewData> ViewData,
									   std::shared_ptr<RenderCore::RHITexture2D> SSRReflectionColor)
	{
		C_P(PostProcessor);
		if (!d->EnableSSR || !d->SSREffect || !SSRReflectionColor)
			return;

		SSRPass SSRPassNode(
			RHIContext,
			SceneTextures,
			ViewPort,
			ViewData,
			d->SSREffect,
			[SSRReflectionColor]() { return SSRReflectionColor; });
		Graph.AddPass(SSRPassNode.BuildDesc());

		Graph.AddPass(d->ApplySSR->BuildDesc(
			RHIContext,
			SceneTextures,
			ViewPort,
			[d]() { return d->SSREffect ? d->SSREffect->GetSSRBuffer() : std::shared_ptr<RenderCore::RHITexture2D>{}; }));
	}

	void PostProcessor::BuildBloomPasses(FRDGBuilder& Graph, RenderCore::RHICommandContext& RHIContext, std::shared_ptr<FSceneTextures> SceneTextures,
										 std::shared_ptr<RenderCore::RHIViewPort> ViewPort, bool UseSSRComposite)
	{
		C_P(PostProcessor);
		const std::string bloomSceneInput = UseSSRComposite ? "SceneColorWithSSR" : "SceneColor";

		RegisterBloomTransientUavs(Graph, SceneTextures);

		BloomPass BloomPassNode(
			RHIContext,
			SceneTextures,
			d->BloomEffect,
			[SceneTextures, UseSSRComposite]() { return UseSSRComposite ? SceneTextures->GetSceneColorWithSSR() : SceneTextures->GetSceneColor(); },
			bloomSceneInput,
			&Graph);
		Graph.AddPass(BloomPassNode.BuildDesc());

		Graph.AddPass(d->ApplyBloom->BuildDesc(
			RHIContext,
			SceneTextures,
			ViewPort,
			bloomSceneInput,
			[SceneTextures, UseSSRComposite]() { return UseSSRComposite ? SceneTextures->GetSceneColorWithSSR() : SceneTextures->GetSceneColor(); },
			[d]() { return d->BloomEffect ? d->BloomEffect->GetResult() : std::shared_ptr<RenderCore::RHITexture2D>{}; }));
	}

	void PostProcessor::BuildAAPasses(FRDGBuilder& Graph, RenderCore::RHICommandContext& RHIContext, std::shared_ptr<FSceneTextures> SceneTextures,
									  std::shared_ptr<RenderCore::RHIViewPort> ViewPort, std::shared_ptr<const FSceneViewData> ViewData,
									  std::shared_ptr<RenderCore::RHITexture2D> AntiAliasingColor)
	{
		C_P(PostProcessor);
		switch (d->AAType)
		{
		case EPostProcessorAAType::TAA:
		{
			TAAPass Pass(RHIContext, SceneTextures, ViewData, d->TAA, [AntiAliasingColor]() { return AntiAliasingColor; });
			Graph.AddPass(Pass.BuildDesc());
			break;
		}
		case EPostProcessorAAType::FXAA:
		{
			FXAAPass Pass(
				RHIContext,
				d->FXaa,
				[AntiAliasingColor]() { return AntiAliasingColor; });
			Graph.AddPass(Pass.BuildDesc());
			break;
		}
		}
	}

	void PostProcessor::BuildTonemappingPass(FRDGBuilder& Graph, RenderCore::RHICommandContext& RHIContext, std::shared_ptr<FSceneTextures> SceneTextures,
											 std::shared_ptr<RenderCore::RHIViewPort> ViewPort)
	{
		C_P(PostProcessor);
		const bool bFxaaTonemap = d->AAType == EPostProcessorAAType::FXAA && d->FXaa;
		// TAA path: bloom composites into SceneColorWithBloom after TAA; FXAA path uses FXAAResult.
		const bool bBloomCompositeTonemap = !bFxaaTonemap && d->BloomEffect && SceneTextures->GetSceneColorWithBloom();
		const std::string tonemapInput = bFxaaTonemap ? "FXAAResult" : (bBloomCompositeTonemap ? "SceneColorWithBloom" : "SceneColor");
		Graph.AddPass(d->Tonemapping->BuildDesc(
			RHIContext,
			SceneTextures,
			ViewPort,
			[d, SceneTextures, bFxaaTonemap, bBloomCompositeTonemap]()
			{
				if (bFxaaTonemap)
					return d->FXaa->GetResult();
				if (bBloomCompositeTonemap)
					return SceneTextures->GetSceneColorWithBloom();
				return SceneTextures->GetSceneColor();
			},
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

	const char* PostProcessor::GetFirstPostProcessPassName() const
	{
		C_P(const PostProcessor);
		if (d->AAType == EPostProcessorAAType::TAA)
			return "TAA";
		if (d->EnableSSR && d->SSREffect)
			return "SSR";
		if (d->BloomEffect)
			return "Bloom";
		if (d->AAType == EPostProcessorAAType::FXAA && d->FXaa)
			return "FXAA";
		return "Tonemapping";
	}

}