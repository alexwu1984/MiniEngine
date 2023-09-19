#include "Render/IBLRender.h"
#include "RHI/DynamicRHI.h"
#include "Engine/Engine.h"
#include "RHI/RHITextureCube.h"
#include "math/matrix4x4.h"
#include "core/system.h"
#include "RHI/RHIShdader.h"
#include "RHI/RHIShaderDefine.h"
#include "Render/MaterialPreFrame.h"

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
		std::shared_ptr< RenderCore::RHIVertexShader> VertexShader;
		std::shared_ptr< RenderCore::RHIPixelShader> IrrPixelShader;
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

		d->PreFilterCube = d->RHI->RHICreateTextureCube(RenderCore::PF_A16B16G16R16, 128,128, 8, false);
		d->IrrCube = d->RHI->RHICreateTextureCube(RenderCore::PF_A16B16G16R16, 256, 256, 5, false);
	}

	void IBLRender::Draw(RenderCore::RHICommandContext& RHIContext)
	{
		C_P(IBLRender);
		d->GET_UNIFORMDATA(CBPerObject).myPerObject_u_mCurrWorld = Matrix4x4();
		//m_programIRR->useShader();

		//for (int i = 0; i < 6; i++)
		//{
		//	m_IBLConstantBuffer.view = captureViews[i];
		//	m_IrrCube->SetRenderTarget(i);
		//	GetDynamicRHI()->SetSamplerState(CC3DPiplelineState::ClampLinerSampler);

		//	GetDynamicRHI()->SetPSShaderResource(0, m_envCube);
		//	GetDynamicRHI()->UpdateConstantBuffer(m_IBLCB, &m_IBLConstantBuffer);
		//	GetDynamicRHI()->SetVSConstantBuffer(0, m_IBLCB);
		//	GetDynamicRHI()->SetPSConstantBuffer(0, m_IBLCB);
		//	renderCube();
		//}
		//GetDynamicRHI()->GenerateMips(m_IrrCube);
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
	}

}
