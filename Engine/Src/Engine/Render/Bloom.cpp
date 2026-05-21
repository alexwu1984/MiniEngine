#include "Render/Bloom.h"
#include "RHI/RHIShdader.h"
#include "RHI/RHIPipeLineState.h"
#include "RHI/RHICachedStates.h"
#include "RHI/DynamicRHI.h"
#include "RHI/RHITexture2D.h"
#include "RHI/RHIRenderTarget.h"
#include "RHI/RHIUnorderedAccessView.h"
#include "Render/SceneTextures.h"
#include "Render/RDGUtils.h"
#include "Render/RenderTexturePool.h"
#include "core/system.h"
#include <algorithm>

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
		std::shared_ptr<RHIUnorderedAccessView> PinBloomChainHead;

		BloomPrivate(DynamicRHI* _RHI) :
			RHI(_RHI),
			GET_SHADER_STRUCT_MEMBER(BloomContants)(_RHI)
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
		InvalidateTransientResources();
		delete d_ptr;
	}

	void Bloom::InitResource()
	{
		C_P(Bloom);

		std::wstring ShaderPath = core::process_directory().wstring() + L"/ShaderLibDX/";
		ShaderPath += L"PostProcess.hlsl";

		d->ExtractBloom = d->RHI->RHICreateComputeShader(ShaderPath, "CS_ExtractBloom", {});
		d->Downsample = d->RHI->RHICreateComputeShader(ShaderPath, "CS_DownSample", {});
		d->UpSample = d->RHI->RHICreateComputeShader(ShaderPath, "CS_UpSample", {});
	}

	void Bloom::InvalidateTransientResources()
	{
		C_P(Bloom);
		if (d->PinBloomChainHead)
		{
			auto Tex = d->PinBloomChainHead->GetTexture2D();
			if (Tex)
			{
				auto Sz = Tex->GetSize();
				RenderTexturePool::Get().ReleaseUAV(Tex->GetPixelFormat(), Sz.x, Sz.y, std::move(d->PinBloomChainHead));
			}
			else
				d->PinBloomChainHead.reset();
		}
		for (int i = 0; i < _countof(d->BloomBuffers); ++i)
		{
			auto& U = d->BloomBuffers[i];
			if (!U)
				continue;
			auto Tex = U->GetTexture2D();
			auto Sz = Tex->GetSize();
			RenderTexturePool::Get().ReleaseUAV(Tex->GetPixelFormat(), Sz.x, Sz.y, std::move(U));
		}
	}

	void Bloom::Draw(RenderCore::RHICommandContext& RHIContext, std::shared_ptr<FSceneTextures> SceneTextures,
					 const std::array<std::shared_ptr<RenderCore::RHIUnorderedAccessView>, 5>& ExternalBloomUavChain)
	{
		C_P(Bloom);
		RenderCore::RHICommandMark Mark(RHIContext, "Bloom");
		auto ScreenSize = SceneTextures->GetSceneColor()->GetSize();

		d->PinBloomChainHead.reset();

		for (int Index = 0; Index < 5; ++Index)
			d->BloomBuffers[Index] = ExternalBloomUavChain[static_cast<size_t>(Index)];

		if (!d->BloomBuffers[0])
			return;

		{
			for (int Index = 0; Index < 5; ++Index)
			{
				RenderCore::ComputePipelineStateInitializer Init;
				Init.ComputeShader = d->Downsample;

				RHIContext.RHISetComputePipelineState(Init);

				if (Index == 0)
				{
					FRDGUtils::RHICmdListDeclareComputeReadableSrvs(RHIContext, { SceneTextures->GetEmissiveBuffer() });
					FRDGUtils::RHICmdListDeclareTextureUavs(RHIContext, { d->BloomBuffers[Index]->GetTexture2D() });
					RHIContext.RHISetShaderTexture(RenderCore::SF_Compute, 0, SceneTextures->GetEmissiveBuffer());
				}
				else
				{
					FRDGUtils::RHICmdListDeclareComputeReadableSrvs(RHIContext, { d->BloomBuffers[Index - 1]->GetTexture2D() });
					FRDGUtils::RHICmdListDeclareTextureUavs(RHIContext, { d->BloomBuffers[Index]->GetTexture2D() });
					RHIContext.RHISetShaderTexture(RenderCore::SF_Compute, 0, d->BloomBuffers[Index - 1]->GetTexture2D());
				}
				RHIContext.RHISetShaderSampler(RenderCore::SF_Compute, 0, RenderCore::RHICachedStates::ClampLinerSampler);
				RHIContext.RHISetUAVParameter(0, d->BloomBuffers[Index]);

				auto TexSize = d->BloomBuffers[Index]->GetTexture2D()->GetSize();
				uint32_t ThreadGroupCountX = (TexSize.w + 7) / 8;
				uint32_t ThreadGroupCountY = (TexSize.h + 7) / 8;
				RHIContext.RHIDispatchComputeShader(ThreadGroupCountX, ThreadGroupCountY, 1);
			}
		}

		//UpSample
		{
			for (int Index = 4; Index > 0; --Index)
			{
				RenderCore::ComputePipelineStateInitializer Init;
				Init.ComputeShader = d->Downsample;

				RHIContext.RHISetComputePipelineState(Init);
				FRDGUtils::RHICmdListDeclareComputeReadableSrvs(RHIContext, { d->BloomBuffers[Index]->GetTexture2D() });
				FRDGUtils::RHICmdListDeclareTextureUavs(RHIContext, { d->BloomBuffers[Index - 1]->GetTexture2D() });
				RHIContext.RHISetShaderSampler(RenderCore::SF_Compute, 0, RenderCore::RHICachedStates::ClampLinerSampler);
				RHIContext.RHISetShaderTexture(RenderCore::SF_Compute, 0, d->BloomBuffers[Index]->GetTexture2D());
				RHIContext.RHISetUAVParameter(0, d->BloomBuffers[Index-1]);

				auto TexSize = d->BloomBuffers[Index-1]->GetTexture2D()->GetSize();
				uint32_t ThreadGroupCountX = (TexSize.w + 7) / 8;
				uint32_t ThreadGroupCountY = (TexSize.h + 7) / 8;
				RHIContext.RHIDispatchComputeShader(ThreadGroupCountX, ThreadGroupCountY, 1);
			}
		}

		if (d->BloomBuffers[0])
		{
			d->PinBloomChainHead = d->BloomBuffers[0];
			for (int Index = 0; Index < 5; ++Index)
				d->BloomBuffers[Index].reset();
		}
	}

	std::shared_ptr< RenderCore::RHITexture2D> Bloom::GetResult() const
	{
		C_P(const Bloom);
		if (d->PinBloomChainHead && d->PinBloomChainHead->GetTexture2D())
			return d->PinBloomChainHead->GetTexture2D();
		if (!d->BloomBuffers[0])
			return {};
		return d->BloomBuffers[0]->GetTexture2D();
	}

}
