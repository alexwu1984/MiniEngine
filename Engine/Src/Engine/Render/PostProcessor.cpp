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

namespace Engine
{
	using namespace RenderCore;
	struct PostProcessorPrivate
	{
		DynamicRHI* RHI = nullptr;
		std::shared_ptr< RHIVertexShader> VertexShader;
		std::shared_ptr< RHIPixelShader> PixelShader;
		std::shared_ptr< TemporallAA> TAA;
		std::shared_ptr< Bloom> BloomEffect;
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
		d->BloomEffect = std::make_shared<Bloom>(d->RHI);
		d->BloomEffect->InitResource();
	}

	void PostProcessor::Draw(RenderCore::RHICommandContext& RHIContext, std::shared_ptr<GBuffer> TargetBuffer, std::shared_ptr<RHIViewPort> ViewPort)
	{
		C_P(PostProcessor);
		ViewPort->SetRenderTarget();
		RHIContext.SetViewPort(0, 0, ViewPort->GetSize().x, ViewPort->GetSize().y);
		d->TAA->Draw(RHIContext, TargetBuffer);
		d->BloomEffect->Draw(RHIContext, TargetBuffer);
		ViewPort->SetRenderTarget();
		RHIContext.SetViewPort(0, 0, ViewPort->GetSize().x, ViewPort->GetSize().y);
		Tonemapping(RHIContext, TargetBuffer);
	}

	void PostProcessor::Tonemapping(RenderCore::RHICommandContext& RHIContext, std::shared_ptr<GBuffer> TargetBuffer)
	{
		C_P(PostProcessor);
		RenderUtil::RenderFullQuad(RHIContext, TargetBuffer->GetSceneColor(), d->VertexShader, d->PixelShader);
	}

}