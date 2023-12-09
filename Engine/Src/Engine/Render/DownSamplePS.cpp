#include "Render/DownSamplePS.h"
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

	BEGIN_SHADER_STRUCT(DownSampleParam, 0)
		DECLARE_PARAM(math::Vector2, InvSize)
		DECLARE_PARAM(int32_t, MipLevel)
		DECLARE_PARAM(int32_t, Pad)
		BEGIN_STRUCT_CONSTRUCT(DownSampleParam)
		END_STRUCT_CONSTRUCT
	END_SHADER_STRUCT

	
	struct DownSamplePSPrivate
	{
		DynamicRHI* RHI = nullptr;
		std::shared_ptr< RHIVertexShader> VertexShader;
		std::shared_ptr< RHIPixelShader> PixelShader;
		std::shared_ptr< RHIRenderTarget> DownSampleTarget;
		std::shared_ptr< RHITexture2D> Tmp;
		int32_t MipLevel = 5;
		core::vec2i Size;

		DownSamplePSPrivate(DynamicRHI* _RHI) :
			GET_SHADER_STRUCT_MEMBER(DownSampleParam)(_RHI)
		{

		}
		DECLARE_SHADER_STRUCT_MEMBER(DownSampleParam);
	};

	DownSamplePS::DownSamplePS(DynamicRHI* RHI)
		:d_ptr(new DownSamplePSPrivate(RHI))
	{
		C_P(DownSamplePS);
		d->RHI = RHI;
	}

	DownSamplePS::~DownSamplePS()
	{
		delete d_ptr;
	}

	void DownSamplePS::InitResource()
	{
		C_P(DownSamplePS);
		std::wstring ShaderPath = core::process_directory().wstring() + L"/ShaderLibDX/";
		ShaderPath += L"PostProcess.hlsl";

		d->VertexShader = d->RHI->RHICreateVertexShader(ShaderPath, "VS_ScreenQuad", {}, {});
		d->PixelShader = d->RHI->RHICreatePixelShader(ShaderPath, "PS_DownSample", {});
	}

	void DownSamplePS::Draw(RHICommandContext& RHIContext, std::shared_ptr<GBuffer> TargetBuffer)
	{
		C_P(DownSamplePS);

		auto SceneColor = TargetBuffer->GetSceneColor();
		d->Size = SceneColor->GetSize();
		if (!d->DownSampleTarget)
		{
			d->DownSampleTarget = d->RHI->RHICreateRenderTarget(SceneColor->GetPixelFormat(),
				d->Size.cx >> 1, d->Size.cy >> 1,d->MipLevel, false, true);
		}

		for (int32_t IndexMip = 0; IndexMip < d->MipLevel; IndexMip++)
		{

			RHIContext.SetRenderTarget(d->DownSampleTarget,IndexMip);
			RHIContext.SetViewPort(0, 0, d->Size.cx >> (IndexMip + 1), d->Size.cy >> (IndexMip + 1));

			d->GET_UNIFORMDATA(DownSampleParam).InvSize.x = 1.f / (float)(d->Size.cx >> 1);
			d->GET_UNIFORMDATA(DownSampleParam).InvSize.y = 1.f / (float)(d->Size.cy >> 1);
			d->GET_UNIFORMDATA(DownSampleParam).MipLevel = IndexMip;
			d->GET_SHADER_STRUCT_MEMBER(DownSampleParam).SetShaderUniformBuffer(RenderCore::EShaderFrequency::SF_Pixel);
			d->GET_SHADER_STRUCT_MEMBER(DownSampleParam).UpdateUniformBuffer();
			Engine::RenderUtil::RenderFullQuad(RHIContext, TargetBuffer->GetSceneColor(), d->VertexShader, d->PixelShader);
			
		}
	}

	std::shared_ptr< RHIRenderTarget> DownSamplePS::GetDownSampleTarget()
	{
		C_P(DownSamplePS);
		return d->DownSampleTarget;
	}

}