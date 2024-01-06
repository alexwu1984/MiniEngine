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

	struct BloomPrivate
	{
		DynamicRHI* RHI;
		std::shared_ptr<RenderCore::RHIComputeShader> ExtractBloom;
		std::shared_ptr<RenderCore::RHIComputeShader> Downsample;
		std::shared_ptr<RenderCore::RHIComputeShader> UpSample;

		std::shared_ptr< RHIUnorderedAccessView> BloomBuffers[5];

		BloomPrivate(DynamicRHI* _RHI) :
			GET_SHADER_STRUCT_MEMBER(BloomContants)(_RHI), RHI(_RHI)
		{

		}
		DECLARE_SHADER_STRUCT_MEMBER(BloomContants);
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
				RHIContext.RHISetShaderSampler(RenderCore::SF_Compute, 0, RenderCore::RHICachedStates::ClampPointSampler);
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
			}
		}

		//UpSample
		{
			for (int Index = 4; Index > 0; --Index)
			{
				RenderCore::ComputePipelineStateInitializer Init;
				Init.ComputeShader = d->Downsample;

				RHIContext.RHISetComputePipelineState(Init);
				RHIContext.RHISetShaderSampler(RenderCore::SF_Compute, 0, RenderCore::RHICachedStates::ClampPointSampler);
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