#include "Render/SkyLightEnvironment.h"
#include "Render/SkyLightIBLPrecomputePrivate.h"
#include "RHI/RHICommandContext.h"
#include "RHI/RHIPipeLineState.h"
#include "RHI/RHICachedStates.h"
#include "core/logger.h"

using namespace math;
using namespace RenderCore;

namespace Engine
{
	void FSkyLightIBLPrecompute::CaptureSkyLightCubemap(RenderCore::RHICommandContext& RHIContext)
	{
		C_P(FSkyLightIBLPrecompute);
		RenderCore::RHICommandMark Mark(RHIContext, "SkyLight_CaptureCubemap");
		GraphicsPipelineStateInitializer Init;
		Init.VertexShader = d->VertexShaderLongLatToCube ? d->VertexShaderLongLatToCube : d->VertexShader;
		Init.PixelShader = d->PSLongLatToCube;
		if (!Init.VertexShader || !Init.PixelShader)
		{
			core::LOG(core::log_err, L"CaptureSkyLightCubemap skipped: vertex or pixel shader not created.");
			return;
		}

		Init.BlendState = RHICachedStates::BlendOnAlphaOff;
		Init.DepthStencilState = RHICachedStates::DepthStateDisable;
		Init.RasterizerState = RHICachedStates::RasterizerStateCullNone;

		RHIContext.RHISetGraphicsPipelineState(Init);
		RHIContext.RHISetShaderSampler(RenderCore::SF_Pixel, 0, RHICachedStates::ClampLinerSampler);

		d->GET_UNIFORMDATA(CBPerObject).myPerObject_u_mCurrWorld = Matrix4x4();
		d->GET_SHADER_STRUCT_MEMBER(CBPerObject).UpdateUniformBuffer();
		d->GET_SHADER_STRUCT_MEMBER(CBPerObject).SetShaderUniformBuffer(RenderCore::SF_Vertex);

		Matrix4x4 Proj = Matrix4x4::MatrixPerspectiveFovLH(0.5f * MATH_PI, 1.f, 0.1f, 10.f);
		for (int32_t IndexView = 0; IndexView < 6; ++IndexView)
		{
			d->GET_UNIFORMDATA(CBPerFrame).myPerFrame.CameraCurrViewProj = d->CaptureViews[IndexView] * Proj;
			d->GET_SHADER_STRUCT_MEMBER(CBPerFrame).UpdateUniformBuffer();
			d->GET_SHADER_STRUCT_MEMBER(CBPerFrame).SetShaderUniformBuffer(RenderCore::SF_Vertex);

			RHIContext.SetRenderTarget(d->EvnCube, IndexView, 0);
			RHIContext.Clear(d->EvnCube, IndexView, 0, core::FLinearColor::Black);

			RHIContext.SetViewPort(0, 0, d->EvnCube->GetSize().cx, d->EvnCube->GetSize().cy);
			RHIContext.RHISetShaderTexture(RenderCore::SF_Pixel, 0, d->HDRTex);
			RenderCube(RHIContext);
		}
		RHIContext.GenerateMips(d->EvnCube);
	}

	void FSkyLightIBLPrecompute::GenerateDiffuseIrradiance(RenderCore::RHICommandContext& RHIContext)
	{
		C_P(FSkyLightIBLPrecompute);
		RenderCore::RHICommandMark Mark(RHIContext, "SkyLight_GenerateDiffuseIrradiance");
		GraphicsPipelineStateInitializer Init;
		Init.VertexShader = d->VertexShader;
		Init.PixelShader = d->IrrPixelShader;
		if (!Init.VertexShader || !Init.PixelShader)
		{
			core::LOG(core::log_err, L"GenerateDiffuseIrradiance skipped: vertex or pixel shader not created.");
			return;
		}

		Init.BlendState = RHICachedStates::BlendOnAlphaOff;
		Init.DepthStencilState = RHICachedStates::DepthStateDisable;
		Init.RasterizerState = RHICachedStates::RasterizerStateCullNone;

		RHIContext.RHISetGraphicsPipelineState(Init);
		RHIContext.RHISetShaderSampler(RenderCore::SF_Pixel, 0, RHICachedStates::ClampLinerSampler);

		d->GET_UNIFORMDATA(CBPerObject).myPerObject_u_mCurrWorld = Matrix4x4();
		d->GET_SHADER_STRUCT_MEMBER(CBPerObject).UpdateUniformBuffer();
		d->GET_SHADER_STRUCT_MEMBER(CBPerObject).SetShaderUniformBuffer(RenderCore::SF_Vertex);

		Matrix4x4 Proj = Matrix4x4::MatrixPerspectiveFovLH(0.5f * MATH_PI, 1.f, 0.1f, 10.f);
		for (int32_t IndexView = 0; IndexView < 6; ++IndexView)
		{
			d->GET_UNIFORMDATA(CBPerFrame).myPerFrame.CameraCurrViewProj = d->CaptureViews[IndexView] * Proj;
			d->GET_SHADER_STRUCT_MEMBER(CBPerFrame).UpdateUniformBuffer();
			d->GET_SHADER_STRUCT_MEMBER(CBPerFrame).SetShaderUniformBuffer(RenderCore::SF_Vertex);

			d->GET_UNIFORMDATA(ENVContant).NumSamplesPerDir = 10;
			d->GET_SHADER_STRUCT_MEMBER(ENVContant).UpdateUniformBuffer();
			d->GET_SHADER_STRUCT_MEMBER(ENVContant).SetShaderUniformBuffer(RenderCore::SF_Pixel);

			RHIContext.SetRenderTarget(d->IrrCube, IndexView, 0);
			RHIContext.Clear(d->IrrCube, IndexView, 0, core::FLinearColor::Black);

			RHIContext.SetViewPort(0, 0, d->IrrCube->GetSize().cx, d->IrrCube->GetSize().cy);
			RHIContext.RHISetShaderTexture(RenderCore::SF_Pixel, 0, d->EvnCube);
			RenderCube(RHIContext);
		}
		RHIContext.GenerateMips(d->IrrCube);
	}

	void FSkyLightIBLPrecompute::GenerateSpecularPrefilter(RenderCore::RHICommandContext& RHIContext)
	{
		C_P(FSkyLightIBLPrecompute);
		RenderCore::RHICommandMark Mark(RHIContext, "SkyLight_GenerateSpecularPrefilter");
		GraphicsPipelineStateInitializer Init;
		Init.VertexShader = d->VertexShader;
		Init.PixelShader = d->PSGenPrefiltered;
		if (!Init.VertexShader || !Init.PixelShader)
		{
			core::LOG(core::log_err, L"GenerateSpecularPrefilter skipped: vertex or pixel shader not created.");
			return;
		}

		Init.BlendState = RHICachedStates::BlendOnAlphaOff;
		Init.DepthStencilState = RHICachedStates::DepthStateDisable;
		Init.RasterizerState = RHICachedStates::RasterizerStateCullNone;

		RHIContext.RHISetGraphicsPipelineState(Init);
		RHIContext.RHISetShaderSampler(RenderCore::SF_Pixel, 0, RHICachedStates::ClampLinerSampler);

		d->GET_UNIFORMDATA(CBPerObject).myPerObject_u_mCurrWorld = Matrix4x4();
		d->GET_SHADER_STRUCT_MEMBER(CBPerObject).UpdateUniformBuffer();
		d->GET_SHADER_STRUCT_MEMBER(CBPerObject).SetShaderUniformBuffer(RenderCore::SF_Vertex);

		Matrix4x4 Proj = Matrix4x4::MatrixPerspectiveFovLH(0.5f * MATH_PI, 1.f, 0.1f, 10.f);
		uint32_t NumMips = d->PreFilterCube->GetNumMips();
		d->GET_UNIFORMDATA(ENVContant).MaxMipLevel = d->PreFilterCube->GetNumMips();

		for (uint32_t MipLevel = 0; MipLevel < NumMips; ++MipLevel)
		{
			uint32_t Size = d->PreFilterCube->GetSize().cx >> MipLevel;

			d->GET_UNIFORMDATA(ENVContant).MipLevel = MipLevel;

			for (int32_t IndexView = 0; IndexView < 6; ++IndexView)
			{
				d->GET_UNIFORMDATA(CBPerFrame).myPerFrame.CameraCurrViewProj = d->CaptureViews[IndexView] * Proj;
				d->GET_SHADER_STRUCT_MEMBER(CBPerFrame).UpdateUniformBuffer();
				d->GET_SHADER_STRUCT_MEMBER(CBPerFrame).SetShaderUniformBuffer(RenderCore::SF_Vertex);

				d->GET_SHADER_STRUCT_MEMBER(ENVContant).UpdateUniformBuffer();
				d->GET_SHADER_STRUCT_MEMBER(ENVContant).SetShaderUniformBuffer(RenderCore::SF_Pixel);

				RHIContext.SetRenderTarget(d->PreFilterCube, IndexView, MipLevel);
				RHIContext.SetViewPort(0, 0, Size, Size);
				RHIContext.Clear(d->PreFilterCube, IndexView, MipLevel, core::FLinearColor::Black);

				RHIContext.RHISetShaderTexture(RenderCore::SF_Pixel, 0, d->EvnCube);
				RenderCube(RHIContext);
			}
		}
	}

	void FSkyLightIBLPrecompute::RenderCube(RenderCore::RHICommandContext& RHIContext)
	{
		C_P(FSkyLightIBLPrecompute);
		d->CubeR->Render(RHIContext);
	}

} // namespace Engine
