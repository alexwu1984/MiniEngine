#include "Render/FXAA.h"
#include "core/system.h"
#include "RHI/RHIShdader.h"
#include "RHI/RHIPipeLineState.h"
#include "RHI/RHICachedStates.h"
#include "RHI/DynamicRHI.h"
#include "RHI/RHITexture2D.h"
#include "RHI/RHIRenderTarget.h"
#include "Render/GBuffer.h"
#include "Render/RenderTexturePool.h"
#include "math/vector2.h"

namespace RenderCore
{

	BEGIN_SHADER_STRUCT(ShaderParameter, 0)
		DECLARE_PARAM(math::Vector2, FXAATexelSize)
		// FXAAEdgeThresholdMin: 0.0833 (default) - 0.0625 (high quality) - 0.0312 (visible limit)
		// Lower values process more pixels (more aggressive anti-aliasing)
		DECLARE_PARAM_VALUE(float, FXAAEdgeThresholdMin, 0.0625f)
		// FXAAEdgeThreshold: 0.166 (default) - 0.125 (high quality) - 0.063 (overkill)
		// Lower values detect more edges (more aggressive anti-aliasing)
		DECLARE_PARAM_VALUE(float, FXAAEdgeThreshold, 0.125f)
		// FXAASubpix: 0.75 (default) - 0.90 (softer, more anti-aliasing) - 1.00 (upper limit)
		// Higher values provide more sub-pixel anti-aliasing
		DECLARE_PARAM_VALUE(float, FXAASubpix, 0.90f)
		DECLARE_PARAM(math::Vector3, pad)
	BEGIN_STRUCT_CONSTRUCT(ShaderParameter)
		END_STRUCT_CONSTRUCT
	END_SHADER_STRUCT

	struct FXAAPrivate
	{
		FXAAPrivate(DynamicRHI* _RHI)
			:RHI(_RHI),
			GET_SHADER_STRUCT_MEMBER(ShaderParameter)(_RHI)
		{

		}
		DynamicRHI* RHI;
		std::shared_ptr<RHIVertexShader> VertexShader;
		std::shared_ptr<RHIPixelShader> PixelShader;
		std::shared_ptr<RHIRenderTarget> FxaaRT;
		DECLARE_SHADER_STRUCT_MEMBER(ShaderParameter);
	};

	FXAA::FXAA(DynamicRHI* RHI)
		:d_ptr(new FXAAPrivate(RHI))
	{
		
	}

	FXAA::~FXAA()
	{
		InvalidateTransientResources();
		delete d_ptr;
	}

	void FXAA::InitResource()
	{
		C_P(FXAA);
		std::wstring ShaderPath = core::process_directory().wstring() + L"/ShaderLibDX/";
		std::wstring VSShaderPath = ShaderPath + L"PostProcess.hlsl";
		d->VertexShader = d->RHI->RHICreateVertexShader(VSShaderPath, "VS_ScreenQuad", {}, {});
		std::wstring PSShaderPath = ShaderPath + L"FXAA.xsf";
		std::vector< RHIShaderMacro> ShaderMacros;
		ShaderMacros.push_back({"XIN_FXAA_QUALITY_LEVEL","16"});
		ShaderMacros.push_back({"XIN_FXAA_CONSOLE","0"});
		ShaderMacros.push_back({"FXAA_GREEN_AS_LUMA","0"});
		d->PixelShader = d->RHI->RHICreatePixelShader(PSShaderPath, "FXAA_3_11_PixelShader", ShaderMacros);
	}

	void FXAA::InvalidateTransientResources()
	{
		C_P(FXAA);
		if (!d->FxaaRT)
			return;
		auto Tex = d->FxaaRT->GetTex();
		if (!Tex)
		{
			d->FxaaRT.reset();
			return;
		}
		auto Sz = Tex->GetSize();
		Engine::RenderTexturePool::Get().ReleaseRenderTarget(
			Tex->GetPixelFormat(), Sz.x, Sz.y, 1, false, false, std::move(d->FxaaRT));
	}

	void FXAA::Draw(RHICommandContext& RHIContext, std::shared_ptr<RHITexture2D> TargetBuffer)
	{
		C_P(FXAA);
		RHICommandMark Mark(RHIContext, "FXAA");
		if (!TargetBuffer)
			return;
		const auto InSize = TargetBuffer->GetSize();
		if (d->FxaaRT)
		{
			auto Tex = d->FxaaRT->GetTex();
			if (!Tex)
				d->FxaaRT.reset();
			else
			{
				auto OutSz = Tex->GetSize();
				if (OutSz.x != InSize.x || OutSz.y != InSize.y || Tex->GetPixelFormat() != TargetBuffer->GetPixelFormat())
				{
					Engine::RenderTexturePool::Get().ReleaseRenderTarget(
						Tex->GetPixelFormat(), OutSz.x, OutSz.y, 1, false, false, std::move(d->FxaaRT));
				}
			}
		}
		if (!d->FxaaRT)
			d->FxaaRT = Engine::RenderTexturePool::Get().AcquireRenderTarget(
				d->RHI, TargetBuffer->GetPixelFormat(), InSize.x, InSize.y, 1, false, false);

		GraphicsPipelineStateInitializer Init;
		Init.VertexShader = d->VertexShader;
		Init.PixelShader = d->PixelShader;
		Init.BlendState = RHICachedStates::BlendDisable;
		Init.DepthStencilState = RHICachedStates::DepthStateDisable;
		Init.RasterizerState = RHICachedStates::RasterizerStateCullNone;
		RHIContext.RHISetGraphicsPipelineState(Init);
		RHIContext.SetRenderTarget(d->FxaaRT);

		RHIContext.RHISetShaderSampler(SF_Pixel, 0, RHICachedStates::ClampPointSampler);
		RHIContext.RHISetShaderTexture(SF_Pixel, 0, TargetBuffer);
		d->GET_UNIFORMDATA(ShaderParameter).FXAATexelSize = { 1.0f/static_cast<float>(TargetBuffer->GetSize().x),
															  1.0f/static_cast<float>(TargetBuffer->GetSize().y) };
		d->GET_UNIFORMDATA(ShaderParameter).FXAAEdgeThresholdMin = 0.0156f;
		d->GET_UNIFORMDATA(ShaderParameter).FXAAEdgeThreshold = 0.0312f;
		d->GET_UNIFORMDATA(ShaderParameter).FXAASubpix = 1.0f;
		d->GET_SHADER_STRUCT_MEMBER(ShaderParameter).UpdateUniformBuffer();
		d->GET_SHADER_STRUCT_MEMBER(ShaderParameter).SetShaderUniformBuffer(EShaderFrequency::SF_Pixel);
		RHIContext.Draw(3);
	}

	std::shared_ptr<RHITexture2D> FXAA::GetResult() const
	{
		C_P(const FXAA);
		if (!d->FxaaRT)
			return {};
		return d->FxaaRT->GetTex();
	}

}