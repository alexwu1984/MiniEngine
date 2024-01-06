#include "Render/Bloom.h"
#include "RHI/RHIShdader.h"
#include "RHI/RHIPipeLineState.h"
#include "RHI/RHICachedStates.h"
#include "RHI/DynamicRHI.h"
#include "RHI/RHITexture2D.h"
#include "RHI/RHIRenderTarget.h"
#include "RHI/RHIUnorderedAccessView.h"
#include "Render/GBuffer.h"
#include "core/system.h"

namespace Engine
{
	using namespace RenderCore;

	struct BlurParam
	{
		math::Vector2 Dir;
		int32_t MipLevel = 0;
		int32_t pad = 0;
	};

	BEGIN_SHADER_STRUCT(CBBlurParam, 0)
		DECLARE_PARAM(BlurParam, Param)
	BEGIN_STRUCT_CONSTRUCT(CBBlurParam)
		END_STRUCT_CONSTRUCT
	END_SHADER_STRUCT

	struct BloomPrivate
	{
		DynamicRHI* RHI;
		std::shared_ptr<RenderCore::RHIComputeShader> ExtractBloom;
		std::shared_ptr<RenderCore::RHIComputeShader> Downsample;
		std::shared_ptr<RenderCore::RHIComputeShader> UpSample;
		std::shared_ptr<RenderCore::RHIComputeShader> Blur;

		std::shared_ptr< RHIUnorderedAccessView> BloomBuffers[5];
		std::shared_ptr< RHIUnorderedAccessView> BlurHorizontalBuffers[5];
		std::shared_ptr< RHIUnorderedAccessView> BlurVerticalBuffers[5];

		BloomPrivate(DynamicRHI* _RHI) :
			RHI(_RHI),
			GET_SHADER_STRUCT_MEMBER(BloomContants)(_RHI),
			GET_SHADER_STRUCT_MEMBER(CBBlurParam)(_RHI)
		{

		}
		DECLARE_SHADER_STRUCT_MEMBER(BloomContants);
		DECLARE_SHADER_STRUCT_MEMBER(CBBlurParam);
	};

	Bloom::Bloom(DynamicRHI* RHI)
		:d_ptr(new BloomPrivate(RHI))
	{

	}

	Bloom::~Bloom()
	{
		delete d_ptr;
	}

	void Bloom::InitResource()
	{
		C_P(Bloom);

		std::wstring ShaderPath = core::process_directory().wstring() + L"/ShaderLibDX/";
		std::wstring TAAShaderPath = ShaderPath + L"PostProcess.hlsl";

		d->ExtractBloom = d->RHI->RHICreateComputeShader(TAAShaderPath, "CS_ExtractBloom", {});
		d->Downsample = d->RHI->RHICreateComputeShader(TAAShaderPath, "CS_DownSample", {});
		d->UpSample = d->RHI->RHICreateComputeShader(TAAShaderPath, "CS_UpSample", {});
		d->Blur = d->RHI->RHICreateComputeShader(TAAShaderPath, "CS_Blur", {});
	}

	void Bloom::Draw(RenderCore::RHICommandContext& RHIContext, std::shared_ptr<GBuffer> TargetBuffer)
	{
		C_P(Bloom);

		auto ScreenSize = TargetBuffer->GetSceneColor()->GetSize();
		auto Size = TargetBuffer->GetSceneColor()->GetSize();
		if (!d->BloomBuffers[0])
		{
			for (int Index = 0; Index < _countof(d->BloomBuffers); ++Index)
			{
				d->BloomBuffers[Index] = d->RHI->RHICreateUnorderedAccessView(EPixelFormat::PF_FloatRGB, Size.x, Size.y);
				d->BlurHorizontalBuffers[Index] = d->RHI->RHICreateUnorderedAccessView(EPixelFormat::PF_FloatRGB, Size.x, Size.y);
				d->BlurVerticalBuffers[Index] = d->RHI->RHICreateUnorderedAccessView(EPixelFormat::PF_FloatRGB, Size.x, Size.y);
				Size.x >>= 1;
				Size.y >>= 1;
			}
		}

		//ExtractBloom
		//{
		//	RenderCore::ComputePipelineStateInitializer Init;
		//	Init.ComputeShader = d->ExtractBloom;

		//	RHIContext.RHISetComputePipelineState(Init);
		//	RHIContext.RHISetShaderSampler(RenderCore::SF_Compute, 0, RenderCore::RHICachedStates::ClampPointSampler);
		//	RHIContext.RHISetShaderTexture(RenderCore::SF_Compute, 0, TargetBuffer->GetSceneColor());
		//	RHIContext.RHISetUAVParameter(0, d->BloomBuffers[0]);

		//	//d->GET_UNIFORMDATA(BloomContants).BloomIntensity = 3.0;
		//	d->GET_SHADER_STRUCT_MEMBER(BloomContants).SetShaderUniformBuffer(RenderCore::EShaderFrequency::SF_Compute);
		//	RHIContext.RHIDispatchComputeShader(ScreenSize.x, ScreenSize.y, 1);
		//}

		//DownSample
		{
			for (int Index = 1; Index < 5; ++Index)
			{
				RenderCore::ComputePipelineStateInitializer Init;
				Init.ComputeShader = d->Downsample;

				RHIContext.RHISetComputePipelineState(Init);
				RHIContext.RHISetShaderSampler(RenderCore::SF_Compute, 0, RenderCore::RHICachedStates::ClampLinerSampler);
				if (Index == 1)
				{
					RHIContext.RHISetShaderTexture(RenderCore::SF_Compute, 0, TargetBuffer->GetEmissiveBuffer());
				}
				else
				{
					RHIContext.RHISetShaderTexture(RenderCore::SF_Compute, 0, d->BloomBuffers[Index-1]->GetTexture2D());
				}
				
				RHIContext.RHISetUAVParameter(0, d->BloomBuffers[Index]);

				auto TexSize = d->BloomBuffers[Index]->GetTexture2D()->GetSize();
				RHIContext.RHIDispatchComputeShader(TexSize.x, TexSize.y, 1);

				//blur
				{
					RenderCore::ComputePipelineStateInitializer BlurPipeline;
					BlurPipeline.ComputeShader = d->Blur;
					RHIContext.RHISetComputePipelineState(BlurPipeline);
					RHIContext.RHISetShaderSampler(RenderCore::SF_Compute, 0, RenderCore::RHICachedStates::ClampLinerSampler);

					d->GET_UNIFORMDATA(CBBlurParam).Param.Dir.x = 1.0f / (float)TexSize.cx;
					d->GET_UNIFORMDATA(CBBlurParam).Param.Dir.y = 0.0f / (float)TexSize.cy;
					d->GET_SHADER_STRUCT_MEMBER(CBBlurParam).SetShaderUniformBuffer(RenderCore::EShaderFrequency::SF_Compute);
					d->GET_SHADER_STRUCT_MEMBER(CBBlurParam).UpdateUniformBuffer();
					RHIContext.RHISetShaderTexture(RenderCore::SF_Compute, 0, d->BloomBuffers[Index]->GetTexture2D());

					RHIContext.RHISetUAVParameter(0, d->BlurHorizontalBuffers[Index]);

					RHIContext.RHIDispatchComputeShader(TexSize.x, TexSize.y, 1);
				}

				{
					RenderCore::ComputePipelineStateInitializer BlurPipeline;
					BlurPipeline.ComputeShader = d->Blur;
					RHIContext.RHISetComputePipelineState(BlurPipeline);
					RHIContext.RHISetShaderSampler(RenderCore::SF_Compute, 0, RenderCore::RHICachedStates::ClampLinerSampler);

					d->GET_UNIFORMDATA(CBBlurParam).Param.Dir.x = 0.0f / (float)TexSize.cx;
					d->GET_UNIFORMDATA(CBBlurParam).Param.Dir.y = 1.0f / (float)TexSize.cy;
					d->GET_SHADER_STRUCT_MEMBER(CBBlurParam).SetShaderUniformBuffer(RenderCore::EShaderFrequency::SF_Compute);
					d->GET_SHADER_STRUCT_MEMBER(CBBlurParam).UpdateUniformBuffer();
					RHIContext.RHISetShaderTexture(RenderCore::SF_Compute, 0, d->BlurHorizontalBuffers[Index]->GetTexture2D());

					RHIContext.RHISetUAVParameter(0, d->BloomBuffers[Index]);

					RHIContext.RHIDispatchComputeShader(TexSize.x, TexSize.y, 1);
				}
			}
		}

		//UpSample
		{
			for (int Index = 4; Index > 0; --Index)
			{
				RenderCore::ComputePipelineStateInitializer Init;
				Init.ComputeShader = d->Downsample;

				RHIContext.RHISetComputePipelineState(Init);
				RHIContext.RHISetShaderSampler(RenderCore::SF_Compute, 0, RenderCore::RHICachedStates::ClampLinerSampler);
				RHIContext.RHISetShaderTexture(RenderCore::SF_Compute, 0, d->BloomBuffers[Index]->GetTexture2D());
				RHIContext.RHISetUAVParameter(0, d->BloomBuffers[Index-1]);

				auto TexSize = d->BloomBuffers[Index-1]->GetTexture2D()->GetSize();
				RHIContext.RHIDispatchComputeShader(TexSize.x, TexSize.y, 1);
			}
		}


	}

	std::shared_ptr< RenderCore::RHITexture2D> Bloom::GetResult() const
	{
		C_P(const Bloom);
		assert(d->BloomBuffers[0].get());
		return d->BloomBuffers[0]->GetTexture2D();
	}

}