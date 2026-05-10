#include "Render/SkyLightEnvironmentSources.h"
#include "math/math.h"
#include "RHI/RHICommandContext.h"
#include "RHI/RHIPipeLineState.h"
#include "RHI/RHICachedStates.h"
#include "core/logger.h"
#include "core/system.h"

#include <chrono>

using namespace math;
using namespace RenderCore;

namespace Engine
{
	void FProceduralSkyEnvironmentSource::InitCubemapPixelShader(DynamicRHI* RHI, const std::wstring& ShaderLibDirectory)
	{
		const std::wstring ProcSkyCubeShaderPath = ShaderLibDirectory + L"SkyAtmosphere.hlsl";
		PSProceduralSkyCube = RHI->RHICreatePixelShader(ProcSkyCubeShaderPath, "PS_ProceduralSkyCube", {});
		if (!PSProceduralSkyCube)
		{
			core::LOG(core::log_err,
					  L"FProceduralSkyEnvironmentSource::InitCubemapPixelShader failed. Check SkyAtmosphere.hlsl.");
		}
	}

	void FProceduralSkyEnvironmentSource::CaptureRadianceCubemap(RHICommandContext& RHIContext, FSkyLightEnvironmentBakePipeline& Bake)
	{
		if (!PSProceduralSkyCube || !Bake.VertexShaderLongLatToCube || !Bake.EvnCube)
			return;

		const auto t0 = std::chrono::steady_clock::now();
		RHICommandMark Mark(RHIContext, "SkyLight_ProceduralSky_CaptureCubemap");
		GraphicsPipelineStateInitializer Init;
		Init.VertexShader = Bake.VertexShaderLongLatToCube;
		Init.PixelShader = PSProceduralSkyCube;
		Init.BlendState = RHICachedStates::BlendOnAlphaOff;
		Init.DepthStencilState = RHICachedStates::DepthStateDisable;
		Init.RasterizerState = RHICachedStates::RasterizerStateCullNone;

		RHIContext.RHISetGraphicsPipelineState(Init);
		RHIContext.RHISetShaderSampler(SF_Pixel, 0, RHICachedStates::ClampLinerSampler);

		Bake.m_CBPerObjectUniformBuffer.Data.myPerObject_u_mCurrWorld = Matrix4x4();
		RHI_UpdateAndBindUniformBuffer(RHIContext, Bake.m_CBPerObjectUniformBuffer, SF_Vertex);

		if (!ProceduralSkyPSCB)
			ProceduralSkyPSCB = Bake.RHI->RHICreateUniformBuffer(64);

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
		p.vSunDirection[0] = ProceduralSunDirX;
		p.vSunDirection[1] = ProceduralSunDirY;
		p.vSunDirection[2] = ProceduralSunDirZ;
		p.rayleigh = 2.0f;
		p.turbidity = 10.0f;
		p.mieCoefficient = 0.005f;
		p.luminance = 1.0f;
		p.mieDirectionalG = 0.8f;
		RHIContext.RHIUpdateUniformBuffer(ProceduralSkyPSCB, &p);
		RHIContext.RHISetShaderUniformBuffer(SF_Pixel, 4, ProceduralSkyPSCB);

		Matrix4x4 Proj = Matrix4x4::MatrixPerspectiveFovLH(0.5f * MATH_PI, 1.f, 0.1f, 10.f);
		for (int32_t IndexView = 0; IndexView < 6; ++IndexView)
		{
			const Matrix4x4 VP = Bake.CaptureViews[IndexView] * Proj;
			Bake.m_CBPerFrameUniformBuffer.Data.myPerFrame.CameraCurrViewProj = VP;
			Bake.m_CBPerFrameUniformBuffer.Data.myPerFrame.CameraCurrViewProjInverse = VP.Inverse();
			RHI_UpdateAndBindUniformBufferVSPS(RHIContext, Bake.m_CBPerFrameUniformBuffer);

			RHIContext.SetRenderTarget(Bake.EvnCube, IndexView, 0);
			RHIContext.Clear(Bake.EvnCube, IndexView, 0, core::FLinearColor::Black);

			RHIContext.SetViewPort(0, 0, Bake.EvnCube->GetSize().cx, Bake.EvnCube->GetSize().cy);
			Bake.RenderCube(RHIContext);
		}
		RHIContext.GenerateMips(Bake.EvnCube);
		const double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
		core::inf() << "IBL: CaptureRadianceCubemap ProceduralSky (GPU 6 faces + mips) " << ms << " ms\n";
	}

	void FSpecifiedCubemapEnvironmentSource::CaptureRadianceCubemap(RHICommandContext& RHIContext, FSkyLightEnvironmentBakePipeline& Bake)
	{
		if (!HDRTex || !Bake.VertexShaderLongLatToCube || !Bake.PSLongLatToCube || !Bake.EvnCube)
			return;

		const auto t0 = std::chrono::steady_clock::now();
		RHICommandMark Mark(RHIContext, "SkyLight_SpecifiedCubemap_CaptureCubemap");
		GraphicsPipelineStateInitializer Init;
		Init.VertexShader = Bake.VertexShaderLongLatToCube;
		Init.PixelShader = Bake.PSLongLatToCube;
		Init.BlendState = RHICachedStates::BlendOnAlphaOff;
		Init.DepthStencilState = RHICachedStates::DepthStateDisable;
		Init.RasterizerState = RHICachedStates::RasterizerStateCullNone;

		RHIContext.RHISetGraphicsPipelineState(Init);
		RHIContext.RHISetShaderSampler(SF_Pixel, 0, RHICachedStates::ClampLinerSampler);

		Bake.m_CBPerObjectUniformBuffer.Data.myPerObject_u_mCurrWorld = Matrix4x4();
		RHI_UpdateAndBindUniformBuffer(RHIContext, Bake.m_CBPerObjectUniformBuffer, SF_Vertex);

		Matrix4x4 Proj = Matrix4x4::MatrixPerspectiveFovLH(0.5f * MATH_PI, 1.f, 0.1f, 10.f);
		for (int32_t IndexView = 0; IndexView < 6; ++IndexView)
		{
			const Matrix4x4 VP = Bake.CaptureViews[IndexView] * Proj;
			Bake.m_CBPerFrameUniformBuffer.Data.myPerFrame.CameraCurrViewProj = VP;
			Bake.m_CBPerFrameUniformBuffer.Data.myPerFrame.CameraCurrViewProjInverse = VP.Inverse();
			RHI_UpdateAndBindUniformBufferVSPS(RHIContext, Bake.m_CBPerFrameUniformBuffer);

			RHIContext.SetRenderTarget(Bake.EvnCube, IndexView, 0);
			RHIContext.Clear(Bake.EvnCube, IndexView, 0, core::FLinearColor::Black);

			RHIContext.SetViewPort(0, 0, Bake.EvnCube->GetSize().cx, Bake.EvnCube->GetSize().cy);
			RHIContext.RHISetShaderTexture(SF_Pixel, 0, HDRTex);
			Bake.RenderCube(RHIContext);
		}
		RHIContext.GenerateMips(Bake.EvnCube);
		const double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
		core::inf() << "IBL: CaptureRadianceCubemap HDR latlong to env cube (GPU 6 faces + mips) " << ms << " ms\n";
	}

} // namespace Engine
