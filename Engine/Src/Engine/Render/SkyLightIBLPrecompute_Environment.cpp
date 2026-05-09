#include "Render/SkyLightEnvironment.h"
#include "Render/SkyLightIBLPrecomputePrivate.h"
#include "RHI/RHICommandContext.h"
#include "RHI/RHIShaderDefine.h"
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
		const bool bProc = d->bProceduralSkyActive && d->PSProceduralSkyCube;
		Init.PixelShader = bProc ? d->PSProceduralSkyCube : d->PSLongLatToCube;
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
		RenderCore::RHI_UpdateAndBindUniformBuffer(RHIContext, d->GET_SHADER_STRUCT_MEMBER(CBPerObject), RenderCore::SF_Vertex);

		if (bProc)
		{
			if (!d->ProceduralSkyPSCB)
				d->ProceduralSkyPSCB = d->RHI->RHICreateUniformBuffer(64);

			// Match Cauldron SkyDomeProc constants (glTFSample src/DX12/Renderer.cpp procedural sky path).
			struct alignas(16) FProcSkyPSParams
			{
				float vSunDirection[3];
				float padding0;
				float rayleigh;
				float turbidity;
				float mieCoefficient;
				float luminance;
				float mieDirectionalG;
				float pad1[3];
			};
			FProcSkyPSParams p{};
			p.vSunDirection[0] = d->ProceduralSunDirX;
			p.vSunDirection[1] = d->ProceduralSunDirY;
			p.vSunDirection[2] = d->ProceduralSunDirZ;
			p.rayleigh = 2.0f;
			p.turbidity = 10.0f;
			p.mieCoefficient = 0.005f;
			p.luminance = 1.0f;
			p.mieDirectionalG = 0.8f;
			RHIContext.RHIUpdateUniformBuffer(d->ProceduralSkyPSCB, &p);
			RHIContext.RHISetShaderUniformBuffer(RenderCore::SF_Pixel, 4, d->ProceduralSkyPSCB);
		}

		Matrix4x4 Proj = Matrix4x4::MatrixPerspectiveFovLH(0.5f * MATH_PI, 1.f, 0.1f, 10.f);
		for (int32_t IndexView = 0; IndexView < 6; ++IndexView)
		{
			const Matrix4x4 VP = d->CaptureViews[IndexView] * Proj;
			d->GET_UNIFORMDATA(CBPerFrame).myPerFrame.CameraCurrViewProj = VP;
			d->GET_UNIFORMDATA(CBPerFrame).myPerFrame.CameraCurrViewProjInverse = VP.Inverse();
			// PS (LongLat / procedural / irradiance / prefilter) reads CameraCurrViewProjInverse from cbPerFrame — bind to pixel stage too.
			RenderCore::RHI_UpdateAndBindUniformBufferVSPS(RHIContext, d->GET_SHADER_STRUCT_MEMBER(CBPerFrame));

			RHIContext.SetRenderTarget(d->EvnCube, IndexView, 0);
			RHIContext.Clear(d->EvnCube, IndexView, 0, core::FLinearColor::Black);

			RHIContext.SetViewPort(0, 0, d->EvnCube->GetSize().cx, d->EvnCube->GetSize().cy);
			if (!bProc)
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
		RenderCore::RHI_UpdateAndBindUniformBuffer(RHIContext, d->GET_SHADER_STRUCT_MEMBER(CBPerObject), RenderCore::SF_Vertex);

		Matrix4x4 Proj = Matrix4x4::MatrixPerspectiveFovLH(0.5f * MATH_PI, 1.f, 0.1f, 10.f);
		for (int32_t IndexView = 0; IndexView < 6; ++IndexView)
		{
			const Matrix4x4 VP = d->CaptureViews[IndexView] * Proj;
			d->GET_UNIFORMDATA(CBPerFrame).myPerFrame.CameraCurrViewProj = VP;
			d->GET_UNIFORMDATA(CBPerFrame).myPerFrame.CameraCurrViewProjInverse = VP.Inverse();
			RenderCore::RHI_UpdateAndBindUniformBufferVSPS(RHIContext, d->GET_SHADER_STRUCT_MEMBER(CBPerFrame));

			d->GET_UNIFORMDATA(ENVContant).NumSamplesPerDir = 10;
			RenderCore::RHI_UpdateAndBindUniformBuffer(RHIContext, d->GET_SHADER_STRUCT_MEMBER(ENVContant), RenderCore::SF_Pixel);

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
		RenderCore::RHI_UpdateAndBindUniformBuffer(RHIContext, d->GET_SHADER_STRUCT_MEMBER(CBPerObject), RenderCore::SF_Vertex);

		Matrix4x4 Proj = Matrix4x4::MatrixPerspectiveFovLH(0.5f * MATH_PI, 1.f, 0.1f, 10.f);
		uint32_t NumMips = d->PreFilterCube->GetNumMips();
		d->GET_UNIFORMDATA(ENVContant).MaxMipLevel = d->PreFilterCube->GetNumMips();

		for (uint32_t MipLevel = 0; MipLevel < NumMips; ++MipLevel)
		{
			uint32_t Size = d->PreFilterCube->GetSize().cx >> MipLevel;

			d->GET_UNIFORMDATA(ENVContant).MipLevel = MipLevel;

			for (int32_t IndexView = 0; IndexView < 6; ++IndexView)
			{
				const Matrix4x4 VP = d->CaptureViews[IndexView] * Proj;
				d->GET_UNIFORMDATA(CBPerFrame).myPerFrame.CameraCurrViewProj = VP;
				d->GET_UNIFORMDATA(CBPerFrame).myPerFrame.CameraCurrViewProjInverse = VP.Inverse();
				RenderCore::RHI_UpdateAndBindUniformBufferVSPS(RHIContext, d->GET_SHADER_STRUCT_MEMBER(CBPerFrame));

				RenderCore::RHI_UpdateAndBindUniformBuffer(RHIContext, d->GET_SHADER_STRUCT_MEMBER(ENVContant), RenderCore::SF_Pixel);

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
