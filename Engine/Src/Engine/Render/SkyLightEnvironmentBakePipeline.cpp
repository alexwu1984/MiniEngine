#include "Render/SkyLightEnvironmentBakePipeline.h"
#include "core/inc.h"
#include "RHI/RHICommandContext.h"
#include "RHI/RHIPipeLineState.h"
#include "RHI/RHICachedStates.h"
#include "RHI/RHIDefinitions.h"
#include "core/logger.h"
#include "math/math.h"
#include <vector>

using namespace math;
using namespace RenderCore;

namespace Engine
{
	namespace
	{
		float G_SmithJointApprox(float a2, float NoV, float NoL)
		{
			float a = math::Sqrt(a2);
			float Vis_SmithV = NoL * (NoV * (1 - a) + a);
			float Vis_SmithL = NoV * (NoL * (1 - a) + a);
			return 0.5f / (Vis_SmithV + Vis_SmithL);
		}
	} // namespace

	void FSkyLightEnvironmentBakePipeline::InitTexturesAndCubeRender()
	{
		CaptureViews = {
			Matrix4x4::MatrixLookAtLH(Vector3(), Vector3::UnitX, Vector3::UnitY),
			Matrix4x4::MatrixLookAtLH(Vector3(), Vector3::NegUnitX, Vector3::UnitY),
			Matrix4x4::MatrixLookAtLH(Vector3(), Vector3::UnitY, Vector3::NegUnitZ),
			Matrix4x4::MatrixLookAtLH(Vector3(), Vector3::NegUnitY, Vector3::UnitZ),
			Matrix4x4::MatrixLookAtLH(Vector3(), Vector3::UnitZ, Vector3::UnitY),
			Matrix4x4::MatrixLookAtLH(Vector3(), Vector3::NegUnitZ, Vector3::UnitY)};
		EvnCube = RHI->RHICreateTextureCube(PF_A16B16G16R16, kSkyLightIBL_CubeMapSize, kSkyLightIBL_CubeMapSize,
											 SkyLightIBL_ComputeNumMips(kSkyLightIBL_CubeMapSize, kSkyLightIBL_CubeMapSize), false);
		PreFilterCube = RHI->RHICreateTextureCube(PF_A16B16G16R16, kSkyLightIBL_IrradianceSize, kSkyLightIBL_IrradianceSize,
												  SkyLightIBL_ComputeNumMips(kSkyLightIBL_IrradianceSize, kSkyLightIBL_IrradianceSize), false);
		IrrCube = RHI->RHICreateTextureCube(PF_A16B16G16R16, kSkyLightIBL_PrefilterSize, kSkyLightIBL_PrefilterSize,
											SkyLightIBL_ComputeNumMips(kSkyLightIBL_PrefilterSize, kSkyLightIBL_PrefilterSize), false);
		CubeR = std::make_shared<CubeRender>(RHI);
		CubeR->InitResource();
	}

	void FSkyLightEnvironmentBakePipeline::InitSharedShaders(const std::wstring& ShaderLibDirectory)
	{
		const std::wstring SkyIblShaderPath = ShaderLibDirectory + L"EnvironmentSkyIBL.hlsl";
		const std::wstring LongLatShaderPath = ShaderLibDirectory + L"IBLLongLatToCube.hlsl";
		RHIVertexDeclare VertexDeclareRHI;
		VertexDeclareRHI.AppendDeclareInput(VertexDeclareInput(0, EVertexElementType::VET_Float3, false));

		VertexShader = RHI->RHICreateVertexShader(SkyIblShaderPath, "VS_SkyCube", VertexDeclareRHI, {});
		VertexShaderLongLatToCube = RHI->RHICreateVertexShader(LongLatShaderPath, "VS_SkyCube", VertexDeclareRHI, {});
		IrrPixelShader = RHI->RHICreatePixelShader(SkyIblShaderPath, "PS_GenIrradiance", {});
		PSLongLatToCube = RHI->RHICreatePixelShader(LongLatShaderPath, "PS_LongLatToCube", {});
		PSGenPrefiltered = RHI->RHICreatePixelShader(SkyIblShaderPath, "PS_GenPrefiltered", {});
		if (!VertexShader || !VertexShaderLongLatToCube || !PSLongLatToCube || !IrrPixelShader || !PSGenPrefiltered)
		{
			core::LOG(core::log_err,
					  L"FSkyLightEnvironmentBakePipeline::InitSharedShaders failed (missing shader). Check ShaderLibDX.");
		}
	}

	void FSkyLightEnvironmentBakePipeline::GenerateBRDFIntegrationLUT()
	{
		if (PreBRDF)
			return;

		const int width = 128;
		const int height = 32;
		std::vector<Vector2> ImageData(static_cast<size_t>(width) * static_cast<size_t>(height));

		for (int y = 0; y < height; ++y)
		{
			float Roughness = (float)(y + 0.5f) / height;
			float m = Roughness * Roughness;
			float m2 = m * m;

			for (int x = 0; x < width; ++x)
			{
				float NoV = (float)(x + 0.5f) / width;

				Vector3 V;
				V.x = math::Sqrt(1.0f - NoV * NoV);
				V.y = 0.0f;
				V.z = NoV;

				float A = 0.0f;
				float B = 0.0f;

				const uint32_t NumSamples = 128;
				for (uint32_t i = 0; i < NumSamples; i++)
				{
					float E1 = (float)i / NumSamples;
					float E2 = (float)math::ReverseBits(i) / (float)0x100000000LL;

					float Phi = 2.0f * math::MATH_PI * E1;
					float CosTheta = math::Sqrt((1.0f - E2) / (1.0f + (m2 - 1.0f) * E2));
					float SinTheta = math::Sqrt(1.0f - CosTheta * CosTheta);

					Vector3 H(SinTheta * math::Cos(Phi), SinTheta * math::Sin(Phi), CosTheta);
					Vector3 L = 2.0f * V.Dot(H) * H - V;

					float NoL = std::max(L.z, 0.0f);
					float NoH = std::max(H.z, 0.0f);
					float VoH = std::max(V.Dot(H), 0.0f);

					if (NoL > 0.0f)
					{
						float Vis = G_SmithJointApprox(m2, NoV, NoL);
						float NoL_Vis_PDF = NoL * Vis * (4.f * VoH / NoH);
						float Fc = math::Pow(1.0f - VoH, 5.f);
						A += NoL_Vis_PDF * (1.0f - Fc);
						B += NoL_Vis_PDF * Fc;
					}
				}

				Vector2& Texel = ImageData[static_cast<size_t>(y) * static_cast<size_t>(width) + static_cast<size_t>(x)];
				Texel.x = A / NumSamples;
				Texel.y = B / NumSamples;
			}
		}

		PreBRDF = RHI->RHICreateTexture2D(EPixelFormat::PF_G32R32F, TexCreate_ShaderResource, width, height, 1, ImageData.data());
	}

	void FSkyLightEnvironmentBakePipeline::GenerateDiffuseIrradiance(RHICommandContext& RHIContext)
	{
		RHICommandMark Mark(RHIContext, "SkyLight_GenerateDiffuseIrradiance");
		GraphicsPipelineStateInitializer Init;
		Init.VertexShader = VertexShader;
		Init.PixelShader = IrrPixelShader;
		if (!Init.VertexShader || !Init.PixelShader)
		{
			core::LOG(core::log_err, L"GenerateDiffuseIrradiance skipped: vertex or pixel shader not created.");
			return;
		}

		Init.BlendState = RHICachedStates::BlendOnAlphaOff;
		Init.DepthStencilState = RHICachedStates::DepthStateDisable;
		Init.RasterizerState = RHICachedStates::RasterizerStateCullNone;

		RHIContext.RHISetGraphicsPipelineState(Init);
		RHIContext.RHISetShaderSampler(SF_Pixel, 0, RHICachedStates::ClampLinerSampler);

		GET_UNIFORMDATA(CBPerObject).myPerObject_u_mCurrWorld = Matrix4x4();
		RHI_UpdateAndBindUniformBuffer(RHIContext, GET_SHADER_STRUCT_MEMBER(CBPerObject), SF_Vertex);

		Matrix4x4 Proj = Matrix4x4::MatrixPerspectiveFovLH(0.5f * MATH_PI, 1.f, 0.1f, 10.f);
		for (int32_t IndexView = 0; IndexView < 6; ++IndexView)
		{
			const Matrix4x4 VP = CaptureViews[IndexView] * Proj;
			GET_UNIFORMDATA(CBPerFrame).myPerFrame.CameraCurrViewProj = VP;
			GET_UNIFORMDATA(CBPerFrame).myPerFrame.CameraCurrViewProjInverse = VP.Inverse();
			RHI_UpdateAndBindUniformBufferVSPS(RHIContext, GET_SHADER_STRUCT_MEMBER(CBPerFrame));

			GET_UNIFORMDATA(ENVContant).NumSamplesPerDir = 10;
			RHI_UpdateAndBindUniformBuffer(RHIContext, GET_SHADER_STRUCT_MEMBER(ENVContant), SF_Pixel);

			RHIContext.SetRenderTarget(IrrCube, IndexView, 0);
			RHIContext.Clear(IrrCube, IndexView, 0, core::FLinearColor::Black);

			RHIContext.SetViewPort(0, 0, IrrCube->GetSize().cx, IrrCube->GetSize().cy);
			RHIContext.RHISetShaderTexture(SF_Pixel, 0, EvnCube);
			RenderCube(RHIContext);
		}
		RHIContext.GenerateMips(IrrCube);
	}

	void FSkyLightEnvironmentBakePipeline::GenerateSpecularPrefilter(RHICommandContext& RHIContext)
	{
		RHICommandMark Mark(RHIContext, "SkyLight_GenerateSpecularPrefilter");
		GraphicsPipelineStateInitializer Init;
		Init.VertexShader = VertexShader;
		Init.PixelShader = PSGenPrefiltered;
		if (!Init.VertexShader || !Init.PixelShader)
		{
			core::LOG(core::log_err, L"GenerateSpecularPrefilter skipped: vertex or pixel shader not created.");
			return;
		}

		Init.BlendState = RHICachedStates::BlendOnAlphaOff;
		Init.DepthStencilState = RHICachedStates::DepthStateDisable;
		Init.RasterizerState = RHICachedStates::RasterizerStateCullNone;

		RHIContext.RHISetGraphicsPipelineState(Init);
		RHIContext.RHISetShaderSampler(SF_Pixel, 0, RHICachedStates::ClampLinerSampler);

		GET_UNIFORMDATA(CBPerObject).myPerObject_u_mCurrWorld = Matrix4x4();
		RHI_UpdateAndBindUniformBuffer(RHIContext, GET_SHADER_STRUCT_MEMBER(CBPerObject), SF_Vertex);

		Matrix4x4 Proj = Matrix4x4::MatrixPerspectiveFovLH(0.5f * MATH_PI, 1.f, 0.1f, 10.f);
		uint32_t NumMips = PreFilterCube->GetNumMips();
		GET_UNIFORMDATA(ENVContant).MaxMipLevel = PreFilterCube->GetNumMips();

		for (uint32_t MipLevel = 0; MipLevel < NumMips; ++MipLevel)
		{
			uint32_t Size = PreFilterCube->GetSize().cx >> MipLevel;

			GET_UNIFORMDATA(ENVContant).MipLevel = MipLevel;

			for (int32_t IndexView = 0; IndexView < 6; ++IndexView)
			{
				const Matrix4x4 VP = CaptureViews[IndexView] * Proj;
				GET_UNIFORMDATA(CBPerFrame).myPerFrame.CameraCurrViewProj = VP;
				GET_UNIFORMDATA(CBPerFrame).myPerFrame.CameraCurrViewProjInverse = VP.Inverse();
				RHI_UpdateAndBindUniformBufferVSPS(RHIContext, GET_SHADER_STRUCT_MEMBER(CBPerFrame));

				RHI_UpdateAndBindUniformBuffer(RHIContext, GET_SHADER_STRUCT_MEMBER(ENVContant), SF_Pixel);

				RHIContext.SetRenderTarget(PreFilterCube, IndexView, MipLevel);
				RHIContext.SetViewPort(0, 0, Size, Size);
				RHIContext.Clear(PreFilterCube, IndexView, MipLevel, core::FLinearColor::Black);

				RHIContext.RHISetShaderTexture(SF_Pixel, 0, EvnCube);
				RenderCube(RHIContext);
			}
		}
	}

	void FSkyLightEnvironmentBakePipeline::RenderCube(RHICommandContext& RHIContext)
	{
		CubeR->Render(RHIContext);
	}

} // namespace Engine
