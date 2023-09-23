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
#include "tinygltf/json.h"

using namespace math;
using namespace RenderCore;

namespace Engine
{
	BEGIN_SHADER_STRUCT(PSContant, 5)
		DECLARE_PARAM(float, Exposure)
		DECLARE_PARAM(int32_t, MipLevel)
		DECLARE_PARAM(int32_t, MaxMipLevel)
		DECLARE_PARAM(int32_t, NumSamplesPerDir)
		BEGIN_STRUCT_CONSTRUCT(PSContant)
		END_STRUCT_CONSTRUCT
	END_SHADER_STRUCT

	struct IBLRenderPrivate
	{
		std::shared_ptr<RenderCore::RHITextureCube> PreFilterCube;
		std::shared_ptr<RenderCore::RHITextureCube> IrrCube;
		std::shared_ptr< RenderCore::RHITextureCube> EvnCube;
		std::shared_ptr<RenderCore::RHITexture2D> HDRTex;

		std::shared_ptr< RenderCore::RHIVertexShader> VertexShader;
		std::shared_ptr< RenderCore::RHIPixelShader> IrrPixelShader;
		std::shared_ptr< RenderCore::RHIVertexShader> VSLongLatToCube;
		std::shared_ptr< RenderCore::RHIPixelShader> PSLongLatToCube;
		std::shared_ptr< RenderCore::RHIVertexBuffer> CubeVB;
		RenderCore::DynamicRHI* RHI;

		IBLRenderPrivate(RenderCore::DynamicRHI* _RHI)
			:GET_SHADER_STRUCT_MEMBER(PSContant)(_RHI),
			 GET_SHADER_STRUCT_MEMBER(CBPerFrame)(_RHI),
			 GET_SHADER_STRUCT_MEMBER(CBPerObject)(_RHI),
			 RHI(_RHI)
		{

		}

		DECLARE_SHADER_STRUCT_MEMBER(PSContant);
		DECLARE_SHADER_STRUCT_MEMBER(CBPerFrame);
		DECLARE_SHADER_STRUCT_MEMBER(CBPerObject);

		std::array< Matrix4x4, 6> CaptureViews;
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
			Matrix4x4::MatrixLookAtLH(Vector3(),Vector3::NegUnitX,Vector3::UnitY),
			Matrix4x4::MatrixLookAtLH(Vector3(),Vector3::UnitX,Vector3::UnitY),
			Matrix4x4::MatrixLookAtLH(Vector3(),Vector3::UnitY,Vector3::NegUnitZ),
			Matrix4x4::MatrixLookAtLH(Vector3(),Vector3::NegUnitY,Vector3::UnitZ),
			Matrix4x4::MatrixLookAtLH(Vector3(),Vector3::UnitZ,Vector3::UnitY),
			Matrix4x4::MatrixLookAtLH(Vector3(),Vector3::NegUnitZ,Vector3::UnitY)
		};
		d->EvnCube = d->RHI->RHICreateTextureCube(RenderCore::PF_A16B16G16R16, 512, 512, 5, false);
		d->PreFilterCube = d->RHI->RHICreateTextureCube(RenderCore::PF_A16B16G16R16, 128,128, 8, false);
		d->IrrCube = d->RHI->RHICreateTextureCube(RenderCore::PF_A16B16G16R16, 256, 256, 5, false);
	}

	void IBLRender::LoadConfig(const std::wstring& FileName)
	{
		try
		{
			C_P(IBLRender);
			nlohmann::json Root;
			std::ifstream input_json_file(FileName);
			if (!input_json_file.is_open())
			{
				return;
			}

			input_json_file >> Root;
			nlohmann::json GltfJson = Root["Gltf"];
			std::wstring HdrFile = core::process_directory().wstring() + L"/GLTFModel/" + core::u8_ucs2(GltfJson["Hdr"]);
			d->HDRTex = d->RHI->RHICreateHDRTexture2D(HdrFile);
		}
		catch (const std::exception&)
		{

		}
	}

	void IBLRender::Draw(RenderCore::RHICommandContext& RHIContext)
	{
		C_P(IBLRender);
		if (!d->HDRTex)
		{
			return;
		}

		GraphicsPipelineStateInitializer Init;
		Init.VertexShader = d->VertexShader;
		Init.PixelShader = d->PSLongLatToCube;
		
		Init.BlendState = RHICachedStates::BlendOnAlphaOff;
		Init.DepthStencilState = RHICachedStates::DepthStateEnable;
		Init.RasterizerState = RHICachedStates::RasterizerStateCullNone;

		RHIContext.RHISetGraphicsPipelineState(Init);
		RHIContext.RHISetShaderSampler(RenderCore::SF_Pixel, 0, RHICachedStates::ClampLinerSampler);

		//to do,set uniform buffer
		d->GET_UNIFORMDATA(CBPerObject).myPerObject_u_mCurrWorld = Matrix4x4();
		d->GET_SHADER_STRUCT_MEMBER(CBPerObject).UpdateUniformBuffer();
		d->GET_SHADER_STRUCT_MEMBER(CBPerObject).SetShaderUniformBuffer(RenderCore::SF_Vertex);
		d->GET_SHADER_STRUCT_MEMBER(CBPerObject).SetShaderUniformBuffer(RenderCore::SF_Pixel);

		for (size_t IndexView = 0; IndexView < 6; ++IndexView)
		{
			d->GET_UNIFORMDATA(CBPerFrame).myPerFrame.CameraCurrViewProj = d->CaptureViews[IndexView];
			d->GET_SHADER_STRUCT_MEMBER(CBPerFrame).UpdateUniformBuffer();
			d->GET_SHADER_STRUCT_MEMBER(CBPerFrame).SetShaderUniformBuffer(RenderCore::SF_Vertex);
			d->GET_SHADER_STRUCT_MEMBER(CBPerFrame).SetShaderUniformBuffer(RenderCore::SF_Pixel);

			RHIContext.SetRenderTarget(d->EvnCube, IndexView, 0);
			RHIContext.SetViewPort(0, 0, 512, 512);
			RHIContext.RHISetShaderTexture(RenderCore::SF_Pixel, 0, d->HDRTex);
			RenderCube(RHIContext);
		}
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
	}

	void IBLRender::RenderCube(RenderCore::RHICommandContext& RHIContext)
	{
		C_P(IBLRender);
		if (!d->CubeVB )
		{
			float vertices[] = {
				// back face
				-1.0f, -1.0f, -1.0f,
				1.0f, 1.0f, -1.0f,
				1.0f, -1.0f, -1.0f,
				1.0f, 1.0f, -1.0f,
				-1.0f, -1.0f, -1.0f,
				-1.0f, 1.0f, -1.0f,
				-1.0f, -1.0f, 1.0f,
				1.0f, -1.0f, 1.0f,
				1.0f, 1.0f, 1.0f,
				1.0f, 1.0f, 1.0f,
				-1.0f, 1.0f, 1.0f,
				-1.0f, -1.0f, 1.0f,
				-1.0f, 1.0f, 1.0f,
				-1.0f, 1.0f, -1.0f,
				-1.0f, -1.0f, -1.0f,
				-1.0f, -1.0f, -1.0f,
				-1.0f, -1.0f, 1.0f,
				-1.0f, 1.0f, 1.0f,
				1.0f, 1.0f, 1.0f,
				1.0f, -1.0f, -1.0f,
				1.0f, 1.0f, -1.0f,
				1.0f, -1.0f, -1.0f,
				1.0f, 1.0f, 1.0f,
				1.0f, -1.0f, 1.0f,
				-1.0f, -1.0f, -1.0f,
				1.0f, -1.0f, -1.0f,
				1.0f, -1.0f, 1.0f,
				1.0f, -1.0f, 1.0f,
				-1.0f, -1.0f, 1.0f,
				-1.0f, -1.0f, -1.0f,
				-1.0f, 1.0f, -1.0f,
				1.0f, 1.0f, 1.0f,
				1.0f, 1.0f, -1.0f,
				1.0f, 1.0f, 1.0f,
				-1.0f, 1.0f, -1.0f,
				-1.0f, 1.0f, 1.0f,
			};
			d->CubeVB = d->RHI->RHICreateVertexBuffer(vertices, RenderCore::BUF_Dynamic, sizeof(math::Vector3), 36);
		}
		// render Cube
		RHIContext.DrawPrimitive(d->CubeVB);
	}

}
