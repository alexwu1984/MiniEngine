#include "Render/TemporalAA.h"
#include "RHI/RHIShdader.h"
#include "RHI/RHIShaderDefine.h"
#include "RHI/RHIPipeLineState.h"
#include "RHI/RHICachedStates.h"
#include "RHI/DynamicRHI.h"
#include "RHI/RHITexture2D.h"
#include "RHI/RHIUnorderedAccessView.h"
#include "Render/SceneRendering/SceneViewData.h"
#include "core/system.h"
#include "Render/GBuffer.h"
#include "Render/RenderTexturePool.h"

namespace Engine
{
	BEGIN_SHADER_STRUCT(TAAContants, 0)
		DECLARE_PARAM(math::Vector4, Resolution)
		DECLARE_PARAM_VALUE(int32_t, FrameIndex, 0)
		DECLARE_PARAM(math::Vector3,Pad0)
		DECLARE_PARAM(math::Vector4, CurrentJitterPixels)
	BEGIN_STRUCT_CONSTRUCT(TAAContants)
		END_STRUCT_CONSTRUCT
	END_SHADER_STRUCT

	struct TemporallAAPrivate
	{
		TemporallAAPrivate(RenderCore::DynamicRHI* InRHI)
			:RHI(InRHI)
			, GET_SHADER_STRUCT_MEMBER(TAAContants)(InRHI)
		{

		}
		RenderCore::DynamicRHI* RHI = nullptr;
		std::shared_ptr<RenderCore::RHIComputeShader> TAAMain;
		std::shared_ptr<RenderCore::RHIComputeShader> TAASharpener;
		std::shared_ptr<RenderCore::RHIUnorderedAccessView> TemporalColor[2];

		bool First = true;
		uint32_t FrameIndexMod2 = 0;
		uint32_t LastTemporalHistoryGeneration = ~0u;
		DECLARE_SHADER_STRUCT_MEMBER(TAAContants);
	};

	TemporallAA::TemporallAA(RenderCore::DynamicRHI* RHI)
		:d_ptr(new TemporallAAPrivate(RHI))
	{
	}

	TemporallAA::~TemporallAA()
	{
		InvalidateTransientResources();
		delete d_ptr;
	}

	void TemporallAA::InitResource()
	{
		C_P(TemporallAA);
		std::wstring ShaderPath = core::process_directory().wstring() + L"/ShaderLibDX/";
		std::wstring TAAShaderPath = ShaderPath +  L"TAACS.hlsl";
		d->TAAMain = d->RHI->RHICreateComputeShader(TAAShaderPath, "TAA_Main", {});

		std::wstring TAASharpenerShaderPath = ShaderPath + L"TAASharpenerCS.hlsl";
		d->TAASharpener = d->RHI->RHICreateComputeShader(TAASharpenerShaderPath, "mainCS", {});
	}

	void TemporallAA::InvalidateTransientResources()
	{
		C_P(TemporallAA);
		for (auto& U : d->TemporalColor)
		{
			if (!U)
				continue;
			auto Tex = U->GetTexture2D();
			auto Sz = Tex->GetSize();
			RenderTexturePool::Get().ReleaseUAV(Tex->GetPixelFormat(), Sz.x, Sz.y, std::move(U));
		}
		d->First = true;
	}

	void TemporallAA::Draw(RenderCore::RHICommandContext& RHIContext, std::shared_ptr<GBuffer> TargetBuffer, std::shared_ptr<const FSceneViewData> ViewData)
	{
		C_P(TemporallAA);
		if (!ViewData)
			return;
		RenderCore::RHICommandMark Mark(RHIContext,"TAA");

		d->FrameIndexMod2 = static_cast<uint32_t>(ViewData->FrameIndexMod2);
		uint32_t Dst = d->FrameIndexMod2 ^ 1;

		const uint32_t camGen = ViewData->TemporalHistoryGeneration;
		if (camGen != d->LastTemporalHistoryGeneration)
		{
			d->LastTemporalHistoryGeneration = camGen;
			d->First = true;
		}

		auto SceneColor = TargetBuffer->GetSceneColorWithBloom();
		const auto ScSize = SceneColor->GetSize();
		const float width = static_cast<float>(ScSize.w);
		const float height = static_cast<float>(ScSize.h);
		const int32_t iw = ScSize.x;
		const int32_t ih = ScSize.y;
		const auto ScFmt = SceneColor->GetPixelFormat();

		bool temporalTargetsRebuilt = false;
		auto MatchOrDrop = [&](std::shared_ptr<RenderCore::RHIUnorderedAccessView>& Uav) {
			if (!Uav)
				return;
			auto Tex = Uav->GetTexture2D();
			auto Sz = Tex->GetSize();
			if (Sz.x == iw && Sz.y == ih && Tex->GetPixelFormat() == ScFmt)
				return;
			RenderTexturePool::Get().ReleaseUAV(Tex->GetPixelFormat(), Sz.x, Sz.y, std::move(Uav));
			temporalTargetsRebuilt = true;
		};
		MatchOrDrop(d->TemporalColor[0]);
		MatchOrDrop(d->TemporalColor[1]);
		if (temporalTargetsRebuilt)
			d->First = true;

		if (!d->TemporalColor[0])
			d->TemporalColor[0] = RenderTexturePool::Get().AcquireUAV(d->RHI, ScFmt, iw, ih);

		if (!d->TemporalColor[1])
			d->TemporalColor[1] = RenderTexturePool::Get().AcquireUAV(d->RHI, ScFmt, iw, ih);

		if (!d->TemporalColor[0] || !d->TemporalColor[1])
			return;

		uint32_t ThreadGroupCountX = (SceneColor->GetSize().w + 7) / 8;
		uint32_t ThreadGroupCountY = (SceneColor->GetSize().h + 7) / 8;
		//TAA
		{
			const float rcpWidth = 1.f / width;
			const float rcpHeight = 1.f / height;

			d->GET_UNIFORMDATA(TAAContants).Resolution = math::Vector4(width, height, rcpWidth, rcpHeight);
			d->GET_UNIFORMDATA(TAAContants).FrameIndex = d->First ? 1 : ViewData->FrameIndex;
			const math::Vector4 TemporalAAJitter = ViewData->TemporalAAJitter;
			d->GET_UNIFORMDATA(TAAContants).CurrentJitterPixels = math::Vector4(
				TemporalAAJitter.x * width * 0.5f,
				-TemporalAAJitter.y * height * 0.5f,
				0.0f,
				0.0f);

			d->First = false;

			RenderCore::ComputePipelineStateInitializer Init;
			Init.ComputeShader = d->TAAMain;
			RHIContext.RHISetComputePipelineState(Init);
			RHIContext.RHISetShaderSampler(RenderCore::SF_Compute, 0, RenderCore::RHICachedStates::BoderLinerSampler);
			RHIContext.RHISetShaderTexture(RenderCore::SF_Compute, 0, SceneColor);
			RHIContext.RHISetShaderTexture(RenderCore::SF_Compute, 1, d->TemporalColor[d->FrameIndexMod2]->GetTexture2D());
			RHIContext.RHISetShaderTexture(RenderCore::SF_Compute, 2, TargetBuffer->GetMotionVector());
			RHIContext.RHISetShaderTexture(RenderCore::SF_Compute, 3, TargetBuffer->GetDepth());
			RHIContext.RHISetUAVParameter(0, d->TemporalColor[Dst]);
			d->GET_SHADER_STRUCT_MEMBER(TAAContants).UpdateUniformBuffer();
			d->GET_SHADER_STRUCT_MEMBER(TAAContants).SetShaderUniformBuffer(RenderCore::EShaderFrequency::SF_Compute);
			RHIContext.RHIDispatchComputeShader(ThreadGroupCountX, ThreadGroupCountY, 1);
		}

		//Sharpener
		{
			RenderCore::ComputePipelineStateInitializer Init;
			Init.ComputeShader = d->TAASharpener;
			RHIContext.RHISetComputePipelineState(Init);
			RHIContext.RHISetShaderTexture(RenderCore::SF_Compute, 0, d->TemporalColor[Dst]->GetTexture2D());
			RHIContext.RHISetUAVParameter(0, TargetBuffer->GetSceneColorUAV());

			RHIContext.RHIDispatchComputeShader(ThreadGroupCountX, ThreadGroupCountY, 1);
		}
	}

	std::shared_ptr<RenderCore::RHITexture2D> TemporallAA::GetHistoryBuffer()
	{
		C_P(TemporallAA);
		if (!d->TemporalColor[d->FrameIndexMod2])
			return {};
		return d->TemporalColor[d->FrameIndexMod2]->GetTexture2D();
	}
}