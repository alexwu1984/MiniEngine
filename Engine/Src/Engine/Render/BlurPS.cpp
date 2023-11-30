#include "Render/BlurPS.h"
#include "RHI/RHIShdader.h"
#include "RHI/RHIShaderDefine.h"
#include "RHI/RHIPipeLineState.h"
#include "RHI/RHICachedStates.h"
#include "RHI/DynamicRHI.h"
#include "RHI/RHITexture2D.h"
#include "RHI/RHIRenderTarget.h"
#include "core/system.h"
#include "Render/GBuffer.h"
#include "Render/RenderUtil.h"
#include "math/vector2.h"

namespace Engine
{
	using namespace RenderCore;

	struct BlurParam
	{
		math::Vector2 Dir;
		math::Vector2 Pad;
	};

	BEGIN_SHADER_STRUCT(CBBlurParam, 0)
		DECLARE_PARAM(BlurParam, Param)
		BEGIN_STRUCT_CONSTRUCT(CBBlurParam)
		END_STRUCT_CONSTRUCT
	END_SHADER_STRUCT

	struct BlurPSPrivate
	{
		DynamicRHI* RHI = nullptr;
		std::shared_ptr< RHIVertexShader> VertexShader;
		std::shared_ptr< RHIPixelShader> PixelShader;

		BlurPSPrivate(DynamicRHI* _RHI) :
			GET_SHADER_STRUCT_MEMBER(CBBlurParam)(_RHI)
		{

		}
		DECLARE_SHADER_STRUCT_MEMBER(CBBlurParam);
	};

	BlurPS::BlurPS(DynamicRHI* RHI)
		:d_ptr(new BlurPSPrivate(RHI))
	{

	}

	BlurPS::~BlurPS()
	{
		delete d_ptr;
	}

	void BlurPS::InitResource()
	{
		C_P(BlurPS);
		std::wstring ShaderPath = core::process_directory().wstring() + L"/ShaderLibDX/";
		ShaderPath += L"PostProcess.hlsl";

		d->VertexShader = d->RHI->RHICreateVertexShader(ShaderPath, "VS_ScreenQuad", {}, {});
		d->PixelShader = d->RHI->RHICreatePixelShader(ShaderPath, "PS_Blur", {});
	}

	void BlurPS::Draw(RenderCore::RHICommandContext& RHIContext, std::shared_ptr<GBuffer> TargetBuffer)
	{

	}

}