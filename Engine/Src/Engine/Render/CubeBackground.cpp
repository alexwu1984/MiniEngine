#include "Render/CubeBackground.h"
#include "Render/CubeRender.h"
#include "math/matrix4x4.h"
#include "RHI/RHIShaderDefine.h"
#include "RHI/RHIShdader.h"
#include "RHI/DynamicRHI.h"
#include "RHI/RHICachedStates.h"
#include "core/system.h"
#include "Engine/Engine.h"
#include "App/AppWindow.h"

using namespace RenderCore;

namespace Engine
{
	BEGIN_SHADER_STRUCT(CBMatrix, 0)
		DECLARE_PARAM(math::Matrix4x4, View)
		DECLARE_PARAM(math::Matrix4x4, Proj)
		BEGIN_STRUCT_CONSTRUCT(CBMatrix)
		END_STRUCT_CONSTRUCT
	END_SHADER_STRUCT

	struct CubeBackgroundPrivate
	{
		RenderCore::DynamicRHI* RHI;
		std::shared_ptr<CubeRender> CubeR;
		std::shared_ptr< RHIVertexShader> VertexShader;
		std::shared_ptr< RHIPixelShader> PixelShader;
		std::shared_ptr<RHITextureCube> TexCube;
		float xHDRRotate{ 0.f };
		float yHDRRotate{ 1.f };

		CubeBackgroundPrivate(RenderCore::DynamicRHI* _RHI)
			:RHI(_RHI),
			GET_SHADER_STRUCT_MEMBER(CBMatrix)(_RHI)
		{

		}
		DECLARE_SHADER_STRUCT_MEMBER(CBMatrix);
	};

	CubeBackground::CubeBackground(RenderCore::DynamicRHI* RHI)
		:d_ptr(new CubeBackgroundPrivate(RHI))
	{
	}

	CubeBackground::~CubeBackground()
	{
		delete d_ptr;
	}

	void CubeBackground::InitResource()
	{
		C_P(CubeBackground);
		d->CubeR = std::make_shared<CubeRender>(d->RHI);
		d->CubeR->InitResource();
		InitShader();
	}

	void CubeBackground::InitShader()
	{
		C_P(CubeBackground);
		std::wstring ShaderPath = core::process_directory().wstring() + L"/ShaderLibDX/";
		ShaderPath += L"CubeBackground.hlsl";

		RenderCore::RHIVertexDeclare VertexDeclareRHI;
		VertexDeclareRHI.AppendDeclareInput(VertexDeclareInput(0, EVertexElementType::VET_Float3, false));

		d->VertexShader = d->RHI->RHICreateVertexShader(ShaderPath, "VS", VertexDeclareRHI, {});
		d->PixelShader = d->RHI->RHICreatePixelShader(ShaderPath, "PS", {});
	}

	void CubeBackground::Render(RenderCore::RHICommandContext& RHIContext, 
								const std::vector <std::shared_ptr<RenderCore::RHITexture2D>>& Targets, 
								std::shared_ptr<RenderCore::RHITexture2D> Depth)
	{
		C_P(CubeBackground);
		if (!d->TexCube)
			return;
		RenderCore::RHICommandMark Mark(RHIContext, "CubeBackground");
		GraphicsPipelineStateInitializer Init;
		Init.VertexShader = d->VertexShader;
		Init.PixelShader = d->PixelShader;

		Init.BlendState = RHICachedStates::BlendOnAlphaOff;
		Init.DepthStencilState = RHICachedStates::DepthStateDisable;
		Init.RasterizerState = RHICachedStates::RasterizerStateCullNone;

		RHIContext.SetRenderTarget(Targets, Depth);
		int32_t w = GEngine->GetAppWindow()->GetWidth();
		int32_t h = GEngine->GetAppWindow()->GetHeight();
		RHIContext.SetViewPort(0, 0, w, h);
		RHIContext.RHISetGraphicsPipelineState(Init);
		RHIContext.RHISetShaderSampler(RenderCore::SF_Pixel, 0, RHICachedStates::ClampLinerSampler);

		math::Matrix4x4 View = math::Matrix4x4::MatrixLookAtLH(math::Vector3::Zero, math::Vector3::NegUnitZ, math::Vector3::UnitY);
		math::Matrix4x4 Rotate = math::Matrix4x4::RotateX(math::Radians(d->xHDRRotate));
		Rotate *= math::Matrix4x4::RotateY(math::Radians(-d->yHDRRotate));
		math::Matrix4x4 Proj = math::Matrix4x4::MatrixPerspectiveFovLH(math::Radians(45.f), static_cast<float>(w) / h, 0, 100);

		d->GET_UNIFORMDATA(CBMatrix).Proj = Proj;
		d->GET_UNIFORMDATA(CBMatrix).View = View * Rotate;
		d->GET_SHADER_STRUCT_MEMBER(CBMatrix).UpdateUniformBuffer();
		d->GET_SHADER_STRUCT_MEMBER(CBMatrix).SetShaderUniformBuffer(RenderCore::SF_Vertex);
		RHIContext.RHISetShaderTexture(RenderCore::SF_Pixel, 0, d->TexCube);
		d->CubeR->Render(RHIContext);
	}

	void CubeBackground::SetTextureCube(std::shared_ptr<RenderCore::RHITextureCube> TexCube)
	{
		C_P(CubeBackground);
		d->TexCube = TexCube;
	}

	void CubeBackground::SetRotate(float xRotate, float yRotate)
	{
		C_P(CubeBackground);
		d->xHDRRotate = xRotate;
		d->yHDRRotate = yRotate;
	}
}