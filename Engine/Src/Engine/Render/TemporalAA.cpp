#include "Render/TemporalAA.h"
#include "RHI/RHIShdader.h"
#include "RHI/RHIShaderDefine.h"
#include "RHI/RHIPipeLineState.h"
#include "RHI/RHICachedStates.h"
#include "RHI/DynamicRHI.h"
#include "RHI/RHITexture2D.h"
#include "RHI/RHIUnorderedAccessView.h"
#include "core/system.h"
#include "Render/GBuffer.h"

namespace Engine
{

	struct TemporallAAPrivate
	{
		RenderCore::DynamicRHI* RHI = nullptr;
		std::shared_ptr<RenderCore::RHIComputeShader> TAAFirst;
		std::shared_ptr<RenderCore::RHIComputeShader> TAAMain;
		std::shared_ptr<RenderCore::RHIComputeShader> TAASharpener;
		std::shared_ptr<RenderCore::RHIUnorderedAccessView> HistoryUAV;
		std::shared_ptr<RenderCore::RHIUnorderedAccessView> MainOutUAV;
		bool First = true;
	};

	TemporallAA::TemporallAA(RenderCore::DynamicRHI* RHI)
		:d_ptr(new TemporallAAPrivate())
	{
		C_P(TemporallAA);
		d->RHI = RHI;
	}

	TemporallAA::~TemporallAA()
	{
		delete d_ptr;
	}

	void TemporallAA::InitResource()
	{
		C_P(TemporallAA);
		std::wstring ShaderPath = core::process_directory().wstring() + L"/ShaderLibDX/";
		std::wstring TAAShaderPath = ShaderPath +  L"TAA.hlsl";

		d->TAAFirst = d->RHI->RHICreateComputeShader(TAAShaderPath, "first", {});
		d->TAAMain = d->RHI->RHICreateComputeShader(TAAShaderPath, "main", {});

		std::wstring TAASharpenerShaderPath = ShaderPath + L"TAASharpenerCS.hlsl";
		d->TAASharpener = d->RHI->RHICreateComputeShader(TAASharpenerShaderPath, "mainCS", {});
	}

	void TemporallAA::Draw(RenderCore::RHICommandContext& RHIContext, std::shared_ptr<GBuffer> TargetBuffer)
	{
		C_P(TemporallAA);

		auto SceneColor = TargetBuffer->GetSceneColor();

		if (d->First)
		{
			if (!d->HistoryUAV)
			{
				d->HistoryUAV = d->RHI->RHICreateUnorderedAccessView(SceneColor->GetPixelFormat(), SceneColor->GetSize().x, SceneColor->GetSize().y);
			}

			RenderCore::ComputePipelineStateInitializer Init;
			Init.ComputeShader = d->TAAFirst;

			RHIContext.RHISetComputePipelineState(Init);
			RHIContext.RHISetShaderSampler(RenderCore::SF_Compute, 0, RenderCore::RHICachedStates::ClampPointSampler);
			RHIContext.RHISetShaderTexture(RenderCore::SF_Compute, 0, SceneColor);
			RHIContext.RHISetUAVParameter(0, d->HistoryUAV);

			d->First = false;
		}
		else
		{
			if (!d->MainOutUAV)
			{
				d->MainOutUAV = d->RHI->RHICreateUnorderedAccessView(SceneColor->GetPixelFormat(), SceneColor->GetSize().x, SceneColor->GetSize().y);
			}

			RenderCore::ComputePipelineStateInitializer Init;
			Init.ComputeShader = d->TAAMain;

			RHIContext.RHISetComputePipelineState(Init);
			RHIContext.RHISetShaderSampler(RenderCore::SF_Compute, 0, RenderCore::RHICachedStates::ClampPointSampler);
			RHIContext.RHISetShaderSampler(RenderCore::SF_Compute, 1, RenderCore::RHICachedStates::ClampPointSampler);
			RHIContext.RHISetShaderSampler(RenderCore::SF_Compute, 2, RenderCore::RHICachedStates::ClampLinerSampler);
			RHIContext.RHISetShaderSampler(RenderCore::SF_Compute, 3, RenderCore::RHICachedStates::ClampPointSampler);
			
			RHIContext.RHISetShaderTexture(RenderCore::SF_Compute, 0, SceneColor);
			RHIContext.RHISetShaderTexture(RenderCore::SF_Compute, 1, TargetBuffer->GetDepth());
			RHIContext.RHISetShaderTexture(RenderCore::SF_Compute, 2, d->HistoryUAV->GetTextre2D());
			RHIContext.RHISetShaderTexture(RenderCore::SF_Compute, 3, TargetBuffer->GetMotionVector());
			RHIContext.RHISetUAVParameter(0, d->MainOutUAV);
		}

		uint32_t ThreadGroupCountX = (SceneColor->GetSize().w + 15) / 16;
		uint32_t ThreadGroupCountY = (SceneColor->GetSize().h + 15) / 16;

		RHIContext.RHIDispatchComputeShader(ThreadGroupCountX, ThreadGroupCountY, 1);

		//Sharpener
		{
			RenderCore::ComputePipelineStateInitializer Init;
			Init.ComputeShader = d->TAASharpener;
			RHIContext.RHISetComputePipelineState(Init);
			RHIContext.RHISetShaderTexture(RenderCore::SF_Compute, 0, d->MainOutUAV ? d->MainOutUAV->GetTextre2D() : d->HistoryUAV->GetTextre2D());
			RHIContext.RHISetUAVParameter(0, TargetBuffer->GetSceneColorUAV());
			RHIContext.RHISetUAVParameter(1, d->HistoryUAV);

			ThreadGroupCountX = (SceneColor->GetSize().w + 7) / 8;
			ThreadGroupCountY = (SceneColor->GetSize().h + 7) / 8;
			RHIContext.RHIDispatchComputeShader(ThreadGroupCountX, ThreadGroupCountY, 1);
		}
	}

}