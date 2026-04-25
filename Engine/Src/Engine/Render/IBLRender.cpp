#include "Render/IBLRender.h"
#include "RHI/DynamicRHI.h"
#include "Engine/Engine.h"
#include "RHI/RHITextureCube.h"
#include "math/matrix4x4.h"
#include "core/system.h"
#include "RHI/RHIShdader.h"
#include "RHI/RHIShaderDefine.h"
#include "RHI/RHIPipeLineState.h"
#include "RHI/RHICachedStates.h"
#include "Render/MaterialPreFrame.h"
#include "Render/CubeRender.h"

using namespace math;
using namespace RenderCore;

namespace Engine
{
	const int CUBE_MAP_SIZE = 512;
	const int IRRADIANCE_SIZE = 256;
	const int PREFILTERED_SIZE = 256;

	static inline uint32_t ComputeNumMips(uint32_t Width, uint32_t Height)
	{
		uint32_t HighBit;
		_BitScanReverse((unsigned long*)&HighBit, Width | Height);
		return HighBit + 1;
	}

	struct IBLRenderPrivate
	{
		std::shared_ptr<RenderCore::RHITextureCube> PreFilterCube;
		std::shared_ptr<RenderCore::RHITextureCube> IrrCube;
		std::shared_ptr<RenderCore::RHITextureCube> EvnCube;
		std::shared_ptr<RenderCore::RHITexture2D> HDRTex;
		std::shared_ptr<RenderCore::RHITexture2D> PreBRDF;

		std::shared_ptr<RenderCore::RHIVertexShader> VertexShader;
		std::shared_ptr<RenderCore::RHIPixelShader> IrrPixelShader;
		std::shared_ptr<RenderCore::RHIVertexShader> VSLongLatToCube;
		std::shared_ptr<RenderCore::RHIPixelShader> PSLongLatToCube;
		std::shared_ptr<RenderCore::RHIPixelShader> PSGenPrefiltered;
		std::shared_ptr<CubeRender>  CubeR;
		RenderCore::DynamicRHI* RHI;
		std::array<Matrix4x4, 6> CaptureViews;

		IBLRenderPrivate(RenderCore::DynamicRHI* _RHI)
			:GET_SHADER_STRUCT_MEMBER(ENVContant)(_RHI),
			 GET_SHADER_STRUCT_MEMBER(CBPerFrame)(_RHI),
			 GET_SHADER_STRUCT_MEMBER(CBPerObject)(_RHI),
			 RHI(_RHI)
		{

		}

		DECLARE_SHADER_STRUCT_MEMBER(ENVContant);
		DECLARE_SHADER_STRUCT_MEMBER(CBPerFrame);
		DECLARE_SHADER_STRUCT_MEMBER(CBPerObject);
		bool bInitRender = false;
	};

	IBLRender::IBLRender(RenderCore::DynamicRHI* RHI)
		:d_ptr(new IBLRenderPrivate(RHI))
	{

	}

	IBLRender::~IBLRender()
	{
		delete d_ptr;
	}

	void IBLRender::InitResource()
	{
		C_P(IBLRender);

		InitShader();

		d->CaptureViews = {
			Matrix4x4::MatrixLookAtLH(Vector3(),Vector3::UnitX,Vector3::UnitY),
			Matrix4x4::MatrixLookAtLH(Vector3(),Vector3::NegUnitX,Vector3::UnitY),
			Matrix4x4::MatrixLookAtLH(Vector3(),Vector3::UnitY,Vector3::NegUnitZ),
			Matrix4x4::MatrixLookAtLH(Vector3(),Vector3::NegUnitY,Vector3::UnitZ),
			Matrix4x4::MatrixLookAtLH(Vector3(),Vector3::UnitZ,Vector3::UnitY),
			Matrix4x4::MatrixLookAtLH(Vector3(),Vector3::NegUnitZ,Vector3::UnitY)
		};
		d->EvnCube = d->RHI->RHICreateTextureCube(RenderCore::PF_A16B16G16R16, CUBE_MAP_SIZE, CUBE_MAP_SIZE, ComputeNumMips(CUBE_MAP_SIZE, CUBE_MAP_SIZE), false);
		d->PreFilterCube = d->RHI->RHICreateTextureCube(RenderCore::PF_A16B16G16R16, IRRADIANCE_SIZE, IRRADIANCE_SIZE, ComputeNumMips(IRRADIANCE_SIZE, IRRADIANCE_SIZE), false);
		d->IrrCube = d->RHI->RHICreateTextureCube(RenderCore::PF_A16B16G16R16, PREFILTERED_SIZE, PREFILTERED_SIZE, ComputeNumMips(PREFILTERED_SIZE, PREFILTERED_SIZE), false);
		d->CubeR = std::make_shared<CubeRender>(d->RHI);
		d->CubeR->InitResource();
		PreIntegrateBRDF();
	}

	void IBLRender::LoadConfig(const nlohmann::json& Root)
	{
		try
		{
			C_P(IBLRender);
			nlohmann::json EvnJson = Root["Evn"];
			std::wstring HdrFile = core::process_directory().wstring() + L"/GLTFModel/" + core::u8_ucs2(EvnJson["Hdr"]);
			d->HDRTex = d->RHI->RHICreateHDRTexture2D(HdrFile);
		}
		catch (const std::exception&)
		{

		}
	}

	void IBLRender::LoadTex(const std::wstring& FileName)
	{
		C_P(IBLRender);
		d->HDRTex = d->RHI->RHICreateHDRTexture2D(FileName);
		d->bInitRender = false;
	}

	void IBLRender::Draw(RenderCore::RHICommandContext& RHIContext)
	{
		C_P(IBLRender);
		if (!d->HDRTex || d->bInitRender)
		{
			return;
		}
		d->bInitRender = true;
		GenerateCubeMap(RHIContext);
		GenerateIrradianceMap(RHIContext);
		GeneratePrefilteredMap(RHIContext);
	}

	std::shared_ptr<RHITextureCube> IBLRender::GetPreFilterCube()
	{
		C_P(IBLRender);
		return d->PreFilterCube;
	}

	std::shared_ptr<RHITextureCube> IBLRender::GetIrrCube()
	{
		C_P(IBLRender);
		return d->IrrCube;
	}

	std::shared_ptr<RHITextureCube> IBLRender::GetEvnCube()
	{
		C_P(IBLRender);
		return d->EvnCube;
	}

	std::shared_ptr<RHITexture2D> IBLRender::GetPreIntegrateBRDF()
	{
		C_P(IBLRender);
		return d->PreBRDF;
	}

	std::shared_ptr<RHITexture2D> IBLRender::GetHDRTex()
	{
		C_P(IBLRender);
		return d->HDRTex;
	}

	void IBLRender::GenerateCubeMap(RenderCore::RHICommandContext& RHIContext)
	{
		C_P(IBLRender);
		RenderCore::RHICommandMark Mark(RHIContext, "GenerateCubeMap");
		GraphicsPipelineStateInitializer Init;
		Init.VertexShader = d->VertexShader;
		Init.PixelShader = d->PSLongLatToCube;

		Init.BlendState = RHICachedStates::BlendOnAlphaOff;
		Init.DepthStencilState = RHICachedStates::DepthStateDisable;
		Init.RasterizerState = RHICachedStates::RasterizerStateCullNone;

		RHIContext.RHISetGraphicsPipelineState(Init);
		RHIContext.RHISetShaderSampler(RenderCore::SF_Pixel, 0, RHICachedStates::ClampLinerSampler);

		//to do,set uniform buffer
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
			RHIContext.Clear(d->EvnCube, IndexView,0, core::FLinearColor::Black);
			
			RHIContext.SetViewPort(0, 0, d->EvnCube->GetSize().cx, d->EvnCube->GetSize().cy);
			RHIContext.RHISetShaderTexture(RenderCore::SF_Pixel, 0, d->HDRTex);
			RenderCube(RHIContext);
		}
		RHIContext.FlushCommands(false);
		RHIContext.GenerateMips(d->EvnCube);
	}

	void IBLRender::GenerateIrradianceMap(RenderCore::RHICommandContext& RHIContext)
	{
		C_P(IBLRender);
		RenderCore::RHICommandMark Mark(RHIContext, "GenerateIrradianceMap");
		GraphicsPipelineStateInitializer Init;
		Init.VertexShader = d->VertexShader;
		Init.PixelShader = d->IrrPixelShader;

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
			RHIContext.Clear(d->IrrCube, IndexView,0,core::FLinearColor::Black);

			RHIContext.SetViewPort(0, 0, d->IrrCube->GetSize().cx, d->IrrCube->GetSize().cy);
			RHIContext.RHISetShaderTexture(RenderCore::SF_Pixel, 0, d->EvnCube);
			RenderCube(RHIContext);
		}
		RHIContext.FlushCommands(false);
		RHIContext.GenerateMips(d->IrrCube);
	}

	void IBLRender::GeneratePrefilteredMap(RenderCore::RHICommandContext& RHIContext)
	{
		C_P(IBLRender);
		RenderCore::RHICommandMark Mark(RHIContext, "GeneratePrefilteredMap");
		GraphicsPipelineStateInitializer Init;
		Init.VertexShader = d->VertexShader;
		Init.PixelShader = d->PSGenPrefiltered;

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

		RHIContext.RHISetShaderTexture(RenderCore::SF_Pixel, 0, d->EvnCube);

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
				RHIContext.Clear(d->PreFilterCube, IndexView,MipLevel, core::FLinearColor::Black);

				RenderCube(RHIContext);
			}
		}
		RHIContext.FlushCommands(false);
	}

	// Appoximation of joint Smith term for GGX
// [Heitz 2014, "Understanding the Masking-Shadowing Function in Microfacet-Based BRDFs"]
	float G_SmithJointApprox(float a2, float NoV, float NoL)
	{
		float a = math::Sqrt(a2);
		float Vis_SmithV = NoL * (NoV * (1 - a) + a);
		float Vis_SmithL = NoV * (NoL * (1 - a) + a);
		return 0.5f / (Vis_SmithV + Vis_SmithL);
	}

	void IBLRender::PreIntegrateBRDF()
	{
		C_P(IBLRender);
		if (d->PreBRDF)
			return;

		int width = 128; //NoV
		int height = 32; //Roughness
		std::vector<math::Vector2> ImageData(width * height * sizeof(math::Vector2));

		for (int y = 0; y < height; ++y)
		{
			float Roughness = (float)(y + 0.5f) / height;
			float m = Roughness * Roughness;
			float m2 = m * m;

			for (int x = 0; x < width; ++x)
			{
				float NoV = (float)(x + 0.5f) / width;

				math::Vector3 V;
				V.x = math::Sqrt(1.0f - NoV * NoV);	// sin
				V.y = 0.0f;
				V.z = NoV;						// cos

				float A = 0.0f;
				float B = 0.0f;

				const uint32_t NumSamples = 128;
				for (uint32_t i = 0; i < NumSamples; i++)
				{
					float E1 = (float)i / NumSamples;
					float E2 = (float)math::ReverseBits(i) / (float)0x100000000LL;

					{
						float Phi = 2.0f * MATH_PI * E1;
						float CosPhi = math::Cos(Phi);
						float SinPhi = math::Sin(Phi);
						float CosTheta = math::Sqrt((1.0f - E2) / (1.0f + (m2 - 1.0f) * E2));
						float SinTheta = math::Sqrt(1.0f - CosTheta * CosTheta);

						math::Vector3 H(SinTheta * math::Cos(Phi), SinTheta * sin(Phi), CosTheta);
						math::Vector3 L = 2.0f * V.Dot(H) * H - V;

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
				}

				math::Vector2& Texel = ImageData[y * width + x];
				Texel.x = A / NumSamples;
				Texel.y = B / NumSamples;
			}
		}

		d->PreBRDF = d->RHI->RHICreateTexture2D(EPixelFormat::PF_G32R32F, RenderCore::TexCreate_ShaderResource, width, height,1, ImageData.data());
	}

	void IBLRender::InitShader()
	{
		C_P(IBLRender);
		std::wstring ShaderPath = core::process_directory().wstring() + L"/ShaderLibDX/";
		ShaderPath += L"EnvironmentShaders.hlsl";

		RHIVertexDeclare VertexDeclareRHI;
		VertexDeclareRHI.AppendDeclareInput(VertexDeclareInput(0, EVertexElementType::VET_Float3, false));

		d->VertexShader = d->RHI->RHICreateVertexShader(ShaderPath, "VS_SkyCube", VertexDeclareRHI, {});
		d->IrrPixelShader = d->RHI->RHICreatePixelShader(ShaderPath, "PS_GenIrradiance", {});
		d->PSLongLatToCube = d->RHI->RHICreatePixelShader(ShaderPath, "PS_LongLatToCube", {});
		d->PSGenPrefiltered = d->RHI->RHICreatePixelShader(ShaderPath, "PS_GenPrefiltered", {});
	}

	void IBLRender::RenderCube(RenderCore::RHICommandContext& RHIContext)
	{
		C_P(IBLRender);
		d->CubeR->Render(RHIContext);
	}

}
