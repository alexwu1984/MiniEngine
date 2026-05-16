#include "Render/PostProcessPass.h"
#include "core/system.h"
#include "RHI/DynamicRHI.h"
#include "RHI/RHICachedStates.h"
#include "RHI/RHICommandContext.h"
#include "RHI/RHIShaderDefine.h"
#include "RHI/RHIPipeLineState.h"
#include "RHI/RHIShdader.h"
#include "RHI/RHIViewPort.h"
#include "RHI/RHIRenderPass.h"
#include "RHI/RHIUnorderedAccessView.h"
#include "Render/RDGUtils.h"
#include "Render/FXAA.h"
#include "Render/SceneTextures.h"
#include "Render/SSRProcessor.h"
#include "Render/TemporalAA.h"
#include "Render/SceneRendering/SceneViewData.h"

namespace Engine
{
	namespace
	{
		/** Unbind OM before compute passes. */
		void UnbindGraphicsRenderTargets(RenderCore::RHICommandContext& RHIContext)
		{
			FRDGUtils::RHICmdListUnbindAllRenderTargets(RHIContext);
		}

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

	RenderPassDesc TonemappingPass::BuildDesc(RenderCore::RHICommandContext& RHIContext, std::shared_ptr<FSceneTextures> SceneTextures,
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
			[this, &RHIContext, SceneTextures, ViewPort, SourceTexture]() { Execute(RHIContext, SceneTextures, ViewPort, SourceTexture); }
		};
	}

	void TonemappingPass::Execute(RenderCore::RHICommandContext& RHIContext, std::shared_ptr<FSceneTextures> SceneTextures,
								  std::shared_ptr<RenderCore::RHIViewPort> ViewPort,
								  std::function<std::shared_ptr<RenderCore::RHITexture2D>()> SourceTexture) const
	{
		RenderCore::RHICommandMark Mark(RHIContext, "Tonemapping");
		std::shared_ptr<RenderCore::RHITexture2D> BackBuf = ViewPort ? ViewPort->GetBackBuffer() : nullptr;
		if (!BackBuf)
			return;
		std::shared_ptr<RenderCore::RHITexture2D> SrcTex = SourceTexture();
		RenderCore::FRHIRenderPassDesc Om = RenderCore::FRHIRenderPassDesc::SingleColorNoDepth(BackBuf);
		Om.DebugName = "Tonemapping";
		{
			FRDGPassDescriptor B{};
			using A = RenderCore::FRDGResourceAccess;
			if (SrcTex)
				B.Inputs.push_back({ "TonemapSrc", [SrcTex]() { return SrcTex; }, true, A::SRV });
			B.Outputs.push_back({ "BackBuffer", [BackBuf]() { return BackBuf; }, true, A::RTV });
			FRDGUtils::AppendPassTextureBarriers(B, Om.DeclaredTextureBarriers);
		}
		RenderCore::FRHIRenderPassScope RasterScope(RHIContext, std::move(Om));
		RHIContext.RHISetGraphicsPipelineState(CreateFullscreenPipelineState(VertexShader, PixelShader));
		if (BloomConstants)
			RenderCore::RHI_UpdateAndBindUniformBuffer(RHIContext, *BloomConstants, RenderCore::SF_Pixel);
		RHIContext.RHISetShaderSampler(RenderCore::SF_Pixel, 0, RenderCore::RHICachedStates::ClampLinerSampler);
		RHIContext.RHISetShaderTexture(RenderCore::SF_Pixel, 0, SourceTexture());
		RHIContext.Draw(3);
	}

	SSRPass::SSRPass(RenderCore::RHICommandContext& InRHIContext, std::shared_ptr<FSceneTextures> InSceneTextures,
					 std::shared_ptr<RenderCore::RHIViewPort> InViewPort, std::shared_ptr<const FSceneViewData> InViewData,
					 std::shared_ptr<SSRProcessor> InSSR,
					 std::function<std::shared_ptr<RenderCore::RHITexture2D>()> InReflectionColor)
		: RHIContext(InRHIContext)
		, SceneTextures(std::move(InSceneTextures))
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
				{ "Normal", [SceneTextures = SceneTextures]() { return SceneTextures->GetNormalBuffer(); } },
				{ "MetallicRoughness", [SceneTextures = SceneTextures]() { return SceneTextures->GetMetallicRoughnessBuffer(); } },
				{ "Depth", [SceneTextures = SceneTextures]() { return SceneTextures->GetDepth(); } },
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
		SSR->Draw(RHIContext, SceneTextures, ViewPort, ReflectionColor(), ViewData);
	}

	BloomPass::BloomPass(RenderCore::RHICommandContext& InRHIContext, std::shared_ptr<FSceneTextures> InSceneTextures,
						 std::shared_ptr<Bloom> InBloomEffect,
						 std::function<std::shared_ptr<RenderCore::RHITexture2D>()> InSourceTexture,
						 std::string InSceneColorDependencyName,
						 FRDGBuilder* InRDGForPooledBloomUavs)
		: RHIContext(InRHIContext)
		, SceneTextures(std::move(InSceneTextures))
		, BloomEffect(std::move(InBloomEffect))
		, SourceTexture(std::move(InSourceTexture))
		, SceneColorDependencyName(std::move(InSceneColorDependencyName))
		, RDGForPooledBloomUavs(InRDGForPooledBloomUavs)
	{
	}

	RenderPassDesc BloomPass::BuildDesc() const
	{
		FRDGPassDescriptor Desc{};
		Desc.Name = "Bloom";

		FRDGPassResource SceneIn{};
		SceneIn.Name = SceneColorDependencyName;
		SceneIn.Resolve = SourceTexture;
		SceneIn.Required = true;
		SceneIn.Access = FRDGResourceAccess::SRV;

		FRDGPassResource BloomOut{};
		BloomOut.Name = "BloomResult";
		BloomOut.Resolve = [B = BloomEffect]() {
			return B ? B->GetResult() : std::shared_ptr<RenderCore::RHITexture2D>{};
		};
		BloomOut.Required = false;
		BloomOut.Access = FRDGResourceAccess::UAV;

		Desc.Inputs = { std::move(SceneIn) };
		Desc.Outputs = { std::move(BloomOut) };
		Desc.ValidateOutputs = false;
		Desc.PassFlags = RDG_Compute;
		Desc.Queue = ERDGPassQueue::Graphics;
		Desc.bUnbindRenderTargetsBeforeRDGBarriers = true;
		Desc.Execute = [Pass = *this]() { Pass.Execute(); };
		return Desc;
	}

	void BloomPass::Execute() const
	{
		UnbindGraphicsRenderTargets(RHIContext);
		std::array<std::shared_ptr<RenderCore::RHIUnorderedAccessView>, 5> RdgBloomUavs{};
		bool bUseRdgBloomUavs = false;
		if (RDGForPooledBloomUavs)
		{
			bUseRdgBloomUavs = true;
			for (int Idx = 0; Idx < 5; ++Idx)
			{
				RdgBloomUavs[(size_t)Idx] = RDGForPooledBloomUavs->GetTransientUAV("Bloom.Chain" + std::to_string(Idx));
				if (!RdgBloomUavs[(size_t)Idx])
					bUseRdgBloomUavs = false;
			}
		}
		if (bUseRdgBloomUavs)
			BloomEffect->Draw(RHIContext, SceneTextures, &RdgBloomUavs);
		else
			BloomEffect->Draw(RHIContext, SceneTextures, nullptr);
	}

	RenderPassDesc ApplyBloomPass::BuildDesc(RenderCore::RHICommandContext& RHIContext, std::shared_ptr<FSceneTextures> SceneTextures,
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
				{ "SceneColorWithBloom", [SceneTextures = SceneTextures]() { return SceneTextures->GetSceneColorWithBloom(); } }
			},
			[this, &RHIContext, SceneTextures, ViewPort, SourceTexture, BloomTexture]() { Execute(RHIContext, SceneTextures, ViewPort, SourceTexture, BloomTexture); }
		};
	}

	void ApplyBloomPass::Execute(RenderCore::RHICommandContext& RHIContext, std::shared_ptr<FSceneTextures> SceneTextures,
								 std::shared_ptr<RenderCore::RHIViewPort> ViewPort,
								 std::function<std::shared_ptr<RenderCore::RHITexture2D>()> SourceTexture,
								 std::function<std::shared_ptr<RenderCore::RHITexture2D>()> BloomTexture) const
	{
		if (!BloomTexture())
			return;

		RenderCore::RHICommandMark Mark(RHIContext, "ApplyBloom");
		std::shared_ptr<RenderCore::RHITexture2D> Dst = SceneTextures ? SceneTextures->GetSceneColorWithBloom() : nullptr;
		if (!Dst)
			return;
		std::shared_ptr<RenderCore::RHITexture2D> Src0 = SourceTexture();
		std::shared_ptr<RenderCore::RHITexture2D> Src1 = BloomTexture();
		RenderCore::FRHIRenderPassDesc Om = RenderCore::FRHIRenderPassDesc::SingleColorNoDepth(Dst);
		Om.DebugName = "ApplyBloom";
		{
			FRDGPassDescriptor B{};
			using A = RenderCore::FRDGResourceAccess;
			if (Src0)
				B.Inputs.push_back({ "SceneColor", [Src0]() { return Src0; }, true, A::SRV });
			if (Src1)
				B.Inputs.push_back({ "Bloom", [Src1]() { return Src1; }, true, A::SRV });
			B.Outputs.push_back({ "SceneColorWithBloom", [Dst]() { return Dst; }, true, A::RTV });
			FRDGUtils::AppendPassTextureBarriers(B, Om.DeclaredTextureBarriers);
		}
		RenderCore::FRHIRenderPassScope RasterScope(RHIContext, std::move(Om));
		RHIContext.RHISetGraphicsPipelineState(CreateFullscreenPipelineState(VertexShader, PixelShader));
		RHIContext.RHISetShaderSampler(RenderCore::SF_Pixel, 0, RenderCore::RHICachedStates::ClampLinerSampler);
		RHIContext.RHISetShaderTexture(RenderCore::SF_Pixel, 0, SourceTexture());
		RHIContext.RHISetShaderTexture(RenderCore::SF_Pixel, 1, BloomTexture());

		if (BloomConstants)
			RenderCore::RHI_UpdateAndBindUniformBuffer(RHIContext, *BloomConstants, RenderCore::SF_Pixel);

		RHIContext.Draw(3);
	}

	RenderPassDesc ApplySSRPass::BuildDesc(RenderCore::RHICommandContext& RHIContext, std::shared_ptr<FSceneTextures> SceneTextures,
										   std::shared_ptr<RenderCore::RHIViewPort> ViewPort,
										   std::function<std::shared_ptr<RenderCore::RHITexture2D>()> SSRTexture) const
	{
		return {
			"ApplySSR",
			{
				{ "SceneColor", [SceneTextures = SceneTextures]() { return SceneTextures->GetSceneColor(); } },
				{ "SSRBuffer", SSRTexture }
			},
			{
				{ "SceneColorWithSSR", [SceneTextures = SceneTextures]() { return SceneTextures->GetSceneColorWithSSR(); } }
			},
			[this, &RHIContext, SceneTextures, ViewPort, SSRTexture]() { Execute(RHIContext, SceneTextures, ViewPort, SSRTexture); }
		};
	}

	void ApplySSRPass::Execute(RenderCore::RHICommandContext& RHIContext, std::shared_ptr<FSceneTextures> SceneTextures,
							   std::shared_ptr<RenderCore::RHIViewPort> ViewPort,
							   std::function<std::shared_ptr<RenderCore::RHITexture2D>()> SSRTexture) const
	{
		(void)ViewPort;
		if (!SceneTextures)
			return;
		std::shared_ptr<RenderCore::RHITexture2D> SSRTex = SSRTexture();
		if (!SSRTex)
			return;

		RenderCore::RHICommandMark Mark(RHIContext, "ApplySSR");
		std::shared_ptr<RenderCore::RHITexture2D> Dst = SceneTextures->GetSceneColorWithSSR();
		if (!Dst)
			return;
		std::shared_ptr<RenderCore::RHITexture2D> Sc = SceneTextures->GetSceneColor();
		RenderCore::FRHIRenderPassDesc Om = RenderCore::FRHIRenderPassDesc::SingleColorNoDepth(Dst);
		Om.DebugName = "ApplySSR";
		{
			FRDGPassDescriptor B{};
			using A = RenderCore::FRDGResourceAccess;
			if (Sc)
				B.Inputs.push_back({ "SceneColor", [Sc]() { return Sc; }, true, A::SRV });
			B.Inputs.push_back({ "SSRBuffer", [SSRTex]() { return SSRTex; }, true, A::SRV });
			B.Outputs.push_back({ "SceneColorWithSSR", [Dst]() { return Dst; }, true, A::RTV });
			FRDGUtils::AppendPassTextureBarriers(B, Om.DeclaredTextureBarriers);
		}
		RenderCore::FRHIRenderPassScope RasterScope(RHIContext, std::move(Om));
		RHIContext.RHISetGraphicsPipelineState(CreateFullscreenPipelineState(VertexShader, PixelShader));
		RHIContext.RHISetShaderSampler(RenderCore::SF_Pixel, 0, RenderCore::RHICachedStates::ClampLinerSampler);
		RHIContext.RHISetShaderTexture(RenderCore::SF_Pixel, 0, Sc);
		RHIContext.RHISetShaderTexture(RenderCore::SF_Pixel, 1, SSRTex);
		RHIContext.Draw(3);
	}

	TAAPass::TAAPass(RenderCore::RHICommandContext& InRHIContext, std::shared_ptr<FSceneTextures> InSceneTextures,
					 std::shared_ptr<const FSceneViewData> InViewData, std::shared_ptr<TemporallAA> InTAA,
					 std::function<std::shared_ptr<RenderCore::RHITexture2D>()> InSourceTexture)
		: RHIContext(InRHIContext)
		, SceneTextures(std::move(InSceneTextures))
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
				{ "MotionVector", [SceneTextures = SceneTextures]() { return SceneTextures->GetMotionVector(); } },
				{ "Depth", [SceneTextures = SceneTextures]() { return SceneTextures->GetDepth(); } }
			},
			{
				{ "SceneColor", [SceneTextures = SceneTextures]() { return SceneTextures->GetSceneColor(); } }
			},
			[Pass = *this]() { Pass.Execute(); }
		};
	}

	void TAAPass::Execute() const
	{
		UnbindGraphicsRenderTargets(RHIContext);
		TAA->Draw(RHIContext, SceneTextures, ViewData);
	}

	FXAAPass::FXAAPass(RenderCore::RHICommandContext& InRHIContext, std::shared_ptr<FXAA> InFxaaEffect,
					   std::function<std::shared_ptr<RenderCore::RHITexture2D>()> InSourceTexture)
		: RHIContext(InRHIContext)
		, FxaaEffect(std::move(InFxaaEffect))
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
				{ "FXAAResult", [FxaaEffect = FxaaEffect]() { return FxaaEffect ? FxaaEffect->GetResult() : std::shared_ptr<RenderCore::RHITexture2D>{}; }, false }
			},
			[Pass = *this]() { Pass.Execute(); }
		};
	}

	void FXAAPass::Execute() const
	{
		FxaaEffect->Draw(RHIContext, SourceTexture());
	}
}
