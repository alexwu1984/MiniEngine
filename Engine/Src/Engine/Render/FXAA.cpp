#include "Render/FXAA.h"
#include "core/system.h"
#include "RHI/RHIShdader.h"
#include "RHI/RHIPipeLineState.h"
#include "RHI/RHICachedStates.h"
#include "RHI/DynamicRHI.h"
#include "RHI/RHITexture2D.h"
#include "RHI/RHIRenderTarget.h"
#include "Render/GBuffer.h"

namespace Engine
{
	using namespace RenderCore;

	struct FXAAPrivate
	{
		FXAAPrivate(DynamicRHI* _RHI)
			:RHI(_RHI)
		{

		}
		DynamicRHI* RHI;
		std::shared_ptr< RHIVertexShader> VertexShader;
		std::shared_ptr< RHIPixelShader> PixelShader;
	};

	FXAA::FXAA(RenderCore::DynamicRHI* RHI)
		:d_ptr(new FXAAPrivate(RHI))
	{
		
	}

	FXAA::~FXAA()
	{
		delete d_ptr;
	}

	void FXAA::InitResource()
	{
		C_P(FXAA);
		std::wstring ShaderPath = core::process_directory().wstring() + L"/ShaderLibDX/";
		std::wstring VSShaderPath = ShaderPath + L"PostProcess.hlsl";
		d->VertexShader = d->RHI->RHICreateVertexShader(VSShaderPath, "VS_ScreenQuad", {}, {});
		std::wstring PSShaderPath = ShaderPath + L"FXAA.xsf";
		d->PixelShader = d->RHI->RHICreatePixelShader(PSShaderPath, "FXAA_3_11_PixelShader", {});
	}

}