#include "Render/TemporalAA.h"
#include "RHI/RHIShdader.h"
#include "RHI/RHIShaderDefine.h"
#include "RHI/RHIPipeLineState.h"
#include "RHI/RHICachedStates.h"
#include "RHI/DynamicRHI.h"
#include "RHI/RHITexture2D.h"
#include "RHI/RHIUnorderedAccessView.h"
#include "Scene/CameraComponent.h"
#include "core/system.h"
#include "Render/GBuffer.h"

namespace Engine
{
	BEGIN_SHADER_STRUCT(TAAContants, 0)
		DECLARE_PARAM(math::Vector4, Resolution)
		DECLARE_PARAM_VALUE(int32_t, FrameIndex, 0)
		DECLARE_PARAM(math::Vector3,Pad0)
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
		DECLARE_SHADER_STRUCT_MEMBER(TAAContants);
	};

	TemporallAA::TemporallAA(RenderCore::DynamicRHI* RHI)
		:d_ptr(new TemporallAAPrivate(RHI))
	{
	}

	TemporallAA::~TemporallAA()
	{
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

	void TemporallAA::Draw(RenderCore::RHICommandContext& RHIContext, std::shared_ptr<GBuffer> TargetBuffer, std::shared_ptr<CameraComponent> Camera)
	{
		C_P(TemporallAA);

		RenderCore::RHICommandMark Mark(RHIContext,"TAA");

		uint32_t Src = Camera->GetFrameIndexMod2();
		uint32_t Dst = Src ^ 1;

		auto SceneColor = TargetBuffer->GetSceneColorWithBloom();
		const float width = static_cast<float>(SceneColor->GetSize().w);
		const float height = static_cast<float>(SceneColor->GetSize().y);

		if (!d->TemporalColor[0])
			d->TemporalColor[0] = d->RHI->RHICreateUnorderedAccessView(SceneColor->GetPixelFormat(), width, height);

		if (!d->TemporalColor[1])
			d->TemporalColor[1] = d->RHI->RHICreateUnorderedAccessView(SceneColor->GetPixelFormat(), width, height);

		if (!d->TemporalColor[0] || !d->TemporalColor[1])
			return;

		uint32_t ThreadGroupCountX = (SceneColor->GetSize().w + 7) / 8;
		uint32_t ThreadGroupCountY = (SceneColor->GetSize().h + 7) / 8;
		//TAA
		{
			const float rcpWidth = 1.f / width;
			const float rcpHeight = 1.f / height;

			d->GET_UNIFORMDATA(TAAContants).Resolution = math::Vector4(width, height, rcpWidth, rcpHeight);
			d->GET_UNIFORMDATA(TAAContants).FrameIndex = d->First ? 1 : Camera->GetFrameIndex();

			d->First = false;

			RenderCore::ComputePipelineStateInitializer Init;
			Init.ComputeShader = d->TAAMain;
			RHIContext.RHISetComputePipelineState(Init);
			RHIContext.RHISetShaderSampler(RenderCore::SF_Compute, 0, RenderCore::RHICachedStates::BoderLinerSampler);
			RHIContext.RHISetShaderTexture(RenderCore::SF_Compute, 0, SceneColor);
			RHIContext.RHISetShaderTexture(RenderCore::SF_Compute, 1, d->TemporalColor[Src]->GetTexture2D());
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

}