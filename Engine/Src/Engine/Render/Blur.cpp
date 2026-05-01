#include "Render/Blur.h"
#include "RHI/RHIShdader.h"
#include "RHI/RHIShaderDefine.h"
#include "RHI/RHIPipeLineState.h"
#include "RHI/RHICachedStates.h"
#include "RHI/DynamicRHI.h"
#include "RHI/RHITexture2D.h"
#include "RHI/RHIRenderTarget.h"
#include "RHI/RHIUnorderedAccessView.h"
#include "core/system.h"
#include "Render/RenderUtil.h"
#include "Render/RenderTexturePool.h"
#include "math/vector2.h"

namespace Engine
{
	using namespace RenderCore;

	struct BlurParam
	{
		math::Vector2 Dir;
		math::Vector2 Resulution;
		int32_t MipLevel{ 0 };
		int32_t Padding[3]{0};
	};

	BEGIN_SHADER_STRUCT(CBBlurParam, 0)
		DECLARE_PARAM(BlurParam, Param)
		BEGIN_STRUCT_CONSTRUCT(CBBlurParam)
		END_STRUCT_CONSTRUCT
	END_SHADER_STRUCT

	struct BlurPSPrivate
	{
		DynamicRHI* RHI = nullptr;
		std::shared_ptr< RHIVertexShader> VertexShader;
		std::shared_ptr< RHIPixelShader> PixelShader;
		std::shared_ptr< RHIRenderTarget> IntermediateTarget;
		std::shared_ptr< RHIRenderTarget> OutTarget;
		int32_t MipLevel{ 0 };
		core::vec2i Size;

		BlurPSPrivate(DynamicRHI* _RHI) :
			GET_SHADER_STRUCT_MEMBER(CBBlurParam)(_RHI),RHI(_RHI)
		{

		}
		DECLARE_SHADER_STRUCT_MEMBER(CBBlurParam);
	};

	BlurPS::BlurPS(DynamicRHI* RHI)
		:d_ptr(new BlurPSPrivate(RHI))
	{
	}

	BlurPS::~BlurPS()
	{
		C_P(BlurPS);
		if (d->IntermediateTarget)
		{
			auto Tex = d->IntermediateTarget->GetTex();
			auto Sz = Tex->GetSize();
			RenderTexturePool::Get().ReleaseRenderTarget(
				Tex->GetPixelFormat(), Sz.x, Sz.y, 1, false, true, std::move(d->IntermediateTarget));
		}
		if (d->OutTarget)
		{
			auto Tex = d->OutTarget->GetTex();
			auto Sz = Tex->GetSize();
			RenderTexturePool::Get().ReleaseRenderTarget(
				Tex->GetPixelFormat(), Sz.x, Sz.y, 1, false, true, std::move(d->OutTarget));
		}
		delete d_ptr;
	}

	void BlurPS::InitResource(int32_t MipLevel)
	{
		C_P(BlurPS);
		d->MipLevel = MipLevel;
		std::wstring ShaderPath = core::process_directory().wstring() + L"/ShaderLibDX/";
		ShaderPath += L"PostProcess.hlsl";

		d->VertexShader = d->RHI->RHICreateVertexShader(ShaderPath, "VS_ScreenQuad", {}, {});
		d->PixelShader = d->RHI->RHICreatePixelShader(ShaderPath, "PS_Blur", {});
	}

	void BlurPS::Draw(RenderCore::RHICommandContext& RHIContext, std::shared_ptr<RenderCore::RHITexture2D> SrcTex)
	{
		C_P(BlurPS);
		d->Size = SrcTex->GetSize();
		const auto Pf = SrcTex->GetPixelFormat();
		auto MatchRt = [&](std::shared_ptr<RenderCore::RHIRenderTarget>& Rt) {
			if (!Rt)
				return;
			auto Tex = Rt->GetTex();
			auto Sz = Tex->GetSize();
			if (Sz.x == d->Size.cx && Sz.y == d->Size.cy && Tex->GetPixelFormat() == Pf)
				return;
			RenderTexturePool::Get().ReleaseRenderTarget(Tex->GetPixelFormat(), Sz.x, Sz.y, 1, false, true, std::move(Rt));
		};
		MatchRt(d->IntermediateTarget);
		MatchRt(d->OutTarget);

		if (!d->IntermediateTarget)
		{
			d->IntermediateTarget = RenderTexturePool::Get().AcquireRenderTarget(
				d->RHI, Pf, d->Size.cx, d->Size.cy, 1, false, true);
		}

		if (!d->OutTarget)
		{
			d->OutTarget = RenderTexturePool::Get().AcquireRenderTarget(
				d->RHI, Pf, d->Size.cx, d->Size.cy, 1, false, true);
		}

		Draw(RHIContext, SrcTex, d->IntermediateTarget, d->OutTarget, 7);
	}

	void BlurPS::Draw(RenderCore::RHICommandContext& RHIContext, std::shared_ptr<RenderCore::RHITexture2D> InTex,
					 std::shared_ptr<RenderCore::RHIRenderTarget> Intermediate, std::shared_ptr<RenderCore::RHIRenderTarget> OutTex, uint32_t iterations /*= 2*/)
	{
		if (iterations == 0) 
		{
			Step(RHIContext,InTex, OutTex, { 0.0, 0.0 });
			return;
		}

		for (uint32_t i = 0; i < iterations; i++) 
		{
			const auto& inputFb = i ? OutTex->GetTex() : InTex;
			Step(RHIContext,inputFb, Intermediate, { 1.0, 0.0 });
			Step(RHIContext, Intermediate->GetTex(), OutTex, { 0.0, 1.0 });
		}
	}

	std::shared_ptr<RenderCore::RHITexture2D> BlurPS::GetResult() const
	{
		C_P(const BlurPS);
		if (d->OutTarget)
			return d->OutTarget->GetTex();
		return {};
	}

	void BlurPS::Step(RenderCore::RHICommandContext& RHIContext, std::shared_ptr<RenderCore::RHITexture2D> InTex,
		              std::shared_ptr<RenderCore::RHIRenderTarget> OutTex, math::Vector2 Dir)
	{
		C_P(BlurPS);
		RenderCore::GraphicsPipelineStateInitializer Init;
		Init.VertexShader = d->VertexShader;
		Init.PixelShader = d->PixelShader;
		Init.BlendState = RenderCore::RHICachedStates::BlendDisable;
		Init.DepthStencilState = RenderCore::RHICachedStates::DepthStateDisable;
		Init.RasterizerState = RenderCore::RHICachedStates::RasterizerStateCullNone;
		RHIContext.RHISetGraphicsPipelineState(Init);

		d->GET_UNIFORMDATA(CBBlurParam).Param.MipLevel = 0;
		d->GET_UNIFORMDATA(CBBlurParam).Param.Dir = Dir;
		d->GET_UNIFORMDATA(CBBlurParam).Param.Resulution.Set(d->Size.cx, d->Size.cy);
		RHIContext.SetRenderTarget(OutTex);
		RHIContext.Clear(OutTex, core::FLinearColor::Black);
		RHIContext.SetViewPort(0, 0, d->Size.cx, d->Size.cy);

		// Update uniform buffer data first
		d->GET_SHADER_STRUCT_MEMBER(CBBlurParam).UpdateUniformBuffer();
		
		RHIContext.RHISetShaderSampler(RenderCore::SF_Pixel, 0, RenderCore::RHICachedStates::ClampLinerSampler);
		RHIContext.RHISetShaderTexture(RenderCore::SF_Pixel, 0, InTex);
		// Set UniformBuffer after PipelineState to ensure it's not cleared
		d->GET_SHADER_STRUCT_MEMBER(CBBlurParam).SetShaderUniformBuffer(RenderCore::EShaderFrequency::SF_Pixel);
		
		RHIContext.Draw(3);
	}

	struct BlurCSPrivate
	{
		DynamicRHI* RHI = nullptr;
		std::shared_ptr<RenderCore::RHIComputeShader> Blur;
		std::shared_ptr< RHIUnorderedAccessView> BlurHorizontalBuffer;
		int32_t MipLevel = 5;
		core::vec2i Size;

		BlurCSPrivate(DynamicRHI* _RHI) :
			GET_SHADER_STRUCT_MEMBER(CBBlurParam)(_RHI), RHI(_RHI)
		{

		}
		DECLARE_SHADER_STRUCT_MEMBER(CBBlurParam);
	};

	BlurCS::BlurCS(RenderCore::DynamicRHI* RHI)
		:d_ptr(new BlurCSPrivate(RHI))
	{

	}

	BlurCS::~BlurCS()
	{
		C_P(BlurCS);
		if (d->BlurHorizontalBuffer)
		{
			auto Tex = d->BlurHorizontalBuffer->GetTexture2D();
			auto Sz = Tex->GetSize();
			RenderTexturePool::Get().ReleaseUAV(Tex->GetPixelFormat(), Sz.x, Sz.y, std::move(d->BlurHorizontalBuffer));
		}
		delete d_ptr;
	}

	void BlurCS::InitResource()
	{
		C_P(BlurCS);
		std::wstring ShaderPath = core::process_directory().wstring() + L"/ShaderLibDX/";
		ShaderPath += L"PostProcess.hlsl";
		d->Blur = d->RHI->RHICreateComputeShader(ShaderPath, "CS_Blur", {});
	}

	void BlurCS::Dispatch(RenderCore::RHICommandContext& RHIContext, std::shared_ptr<RenderCore::RHITexture2D> SrcTex, std::shared_ptr<RenderCore::RHIUnorderedAccessView> Target)
	{
		C_P(BlurCS);
		auto Size = SrcTex->GetSize();
		const auto Pf = SrcTex->GetPixelFormat();
		if (d->BlurHorizontalBuffer)
		{
			auto Tex = d->BlurHorizontalBuffer->GetTexture2D();
			auto Sz = Tex->GetSize();
			if (Sz.x != Size.x || Sz.y != Size.y || Tex->GetPixelFormat() != Pf)
				RenderTexturePool::Get().ReleaseUAV(Tex->GetPixelFormat(), Sz.x, Sz.y, std::move(d->BlurHorizontalBuffer));
		}
		if (!d->BlurHorizontalBuffer)
			d->BlurHorizontalBuffer = RenderTexturePool::Get().AcquireUAV(d->RHI, Pf, Size.x, Size.y);

		uint32_t ThreadGroupCountX = (Size.w + 7) / 8;
		uint32_t ThreadGroupCountY = (Size.h + 7) / 8;

		{
			RenderCore::ComputePipelineStateInitializer BlurPipeline;
			BlurPipeline.ComputeShader = d->Blur;
			RHIContext.RHISetComputePipelineState(BlurPipeline);
			RHIContext.RHISetShaderSampler(RenderCore::SF_Compute, 0, RenderCore::RHICachedStates::ClampLinerSampler);

			d->GET_UNIFORMDATA(CBBlurParam).Param.Dir.x = 1.0f / (float)Size.cx;
			d->GET_UNIFORMDATA(CBBlurParam).Param.Dir.y = 0.0f / (float)Size.cy;
			d->GET_SHADER_STRUCT_MEMBER(CBBlurParam).SetShaderUniformBuffer(RenderCore::EShaderFrequency::SF_Compute);
			d->GET_SHADER_STRUCT_MEMBER(CBBlurParam).UpdateUniformBuffer();
			RHIContext.RHISetShaderTexture(RenderCore::SF_Compute, 0, SrcTex);
			RHIContext.RHISetUAVParameter(0, d->BlurHorizontalBuffer);

			RHIContext.RHIDispatchComputeShader(ThreadGroupCountX, ThreadGroupCountY, 1);
		}

		{
			RenderCore::ComputePipelineStateInitializer BlurPipeline;
			BlurPipeline.ComputeShader = d->Blur;
			RHIContext.RHISetComputePipelineState(BlurPipeline);
			RHIContext.RHISetShaderSampler(RenderCore::SF_Compute, 0, RenderCore::RHICachedStates::ClampLinerSampler);

			d->GET_UNIFORMDATA(CBBlurParam).Param.Dir.x = 0.0f / (float)Size.cx;
			d->GET_UNIFORMDATA(CBBlurParam).Param.Dir.y = 1.0f / (float)Size.cy;
			d->GET_SHADER_STRUCT_MEMBER(CBBlurParam).SetShaderUniformBuffer(RenderCore::EShaderFrequency::SF_Compute);
			d->GET_SHADER_STRUCT_MEMBER(CBBlurParam).UpdateUniformBuffer();
			RHIContext.RHISetShaderTexture(RenderCore::SF_Compute, 0, d->BlurHorizontalBuffer->GetTexture2D());
			RHIContext.RHISetUAVParameter(0, Target);

			RHIContext.RHIDispatchComputeShader(ThreadGroupCountX, ThreadGroupCountY, 1);
		}
		
	}

}