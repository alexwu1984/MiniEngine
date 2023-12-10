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
		std::shared_ptr< RHIRenderTarget> BlurHorizontalTarget;
		std::shared_ptr< RHIRenderTarget> BlurVerticalTarget;
		int32_t MipLevel = 5;
		core::vec2i Size;

		BlurPSPrivate(DynamicRHI* _RHI) :
			GET_SHADER_STRUCT_MEMBER(CBBlurParam)(_RHI),RHI(_RHI)
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

	void BlurPS::Draw(RenderCore::RHICommandContext& RHIContext,std::shared_ptr<RHITexture2D> SrcTex, std::shared_ptr<GBuffer> TargetBuffe)
	{
		C_P(BlurPS);
		d->Size = SrcTex->GetSize();
		if (!d->BlurHorizontalTarget)
		{
			d->BlurHorizontalTarget = d->RHI->RHICreateRenderTarget(SrcTex->GetPixelFormat(),
				d->Size.cx >> 1, d->Size.cy >> 1, d->MipLevel, false, true);
		}

		if (!d->BlurVerticalTarget)
		{
			d->BlurVerticalTarget = d->RHI->RHICreateRenderTarget(SrcTex->GetPixelFormat(),
				d->Size.cx >> 1, d->Size.cy >> 1, d->MipLevel, false, true);
		}

		for (int IndexMip = 0; IndexMip < d->MipLevel; IndexMip++)
		{
			if (IndexMip == 0)
			{
				Draw(RHIContext, SrcTex, IndexMip);
			}
			else
			{
				Draw(RHIContext, d->BlurVerticalTarget->GetTex(), IndexMip);
			}
			
		}
	}

	void BlurPS::Draw(RenderCore::RHICommandContext& RHIContext, std::shared_ptr<RenderCore::RHITexture2D> SrcTex, int32_t IndexMip)
	{
		C_P(BlurPS);
		{
			RHIContext.SetRenderTarget(d->BlurHorizontalTarget, IndexMip);
			RHIContext.SetViewPort(0, 0, d->Size.cx >> IndexMip, d->Size.cy >> IndexMip);

			d->GET_UNIFORMDATA(CBBlurParam).Param.Dir.x = 1.0f / (float)(d->Size.cx >> IndexMip);
			d->GET_UNIFORMDATA(CBBlurParam).Param.Dir.y = 0.0f / (float)(d->Size.cy >> IndexMip);
			d->GET_SHADER_STRUCT_MEMBER(CBBlurParam).SetShaderUniformBuffer(RenderCore::EShaderFrequency::SF_Pixel);
			d->GET_SHADER_STRUCT_MEMBER(CBBlurParam).UpdateUniformBuffer();
			Engine::RenderUtil::RenderFullQuad(RHIContext, SrcTex, d->VertexShader, d->PixelShader);
		}

		{
			RHIContext.SetRenderTarget(d->BlurVerticalTarget, IndexMip);
			RHIContext.SetViewPort(0, 0, d->Size.cx >> IndexMip, d->Size.cy >> IndexMip);

			d->GET_UNIFORMDATA(CBBlurParam).Param.Dir.x = 0.0f / (float)(d->Size.cx >> IndexMip);
			d->GET_UNIFORMDATA(CBBlurParam).Param.Dir.y = 1.0f / (float)(d->Size.cy >> IndexMip);
			d->GET_SHADER_STRUCT_MEMBER(CBBlurParam).SetShaderUniformBuffer(RenderCore::EShaderFrequency::SF_Pixel);
			d->GET_SHADER_STRUCT_MEMBER(CBBlurParam).UpdateUniformBuffer();
			Engine::RenderUtil::RenderFullQuad(RHIContext, d->BlurHorizontalTarget->GetTex(), d->VertexShader, d->PixelShader);
		}
	}

}