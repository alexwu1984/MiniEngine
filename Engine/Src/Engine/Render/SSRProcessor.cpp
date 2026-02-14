#include "Render/SSRProcessor.h"
#include "RHI/RHIShdader.h"
#include "RHI/RHIShaderDefine.h"
#include "RHI/RHIPipeLineState.h"
#include "RHI/RHICachedStates.h"
#include "RHI/DynamicRHI.h"
#include "RHI/RHITexture2D.h"
#include "RHI/RHIViewPort.h"
#include "Scene/CameraComponent.h"
#include "core/system.h"
#include "math/vector2.h"
#include "Render/GBuffer.h"

using namespace RenderCore;

namespace Engine
{
	BEGIN_SHADER_STRUCT(SSRContants, 0)
		DECLARE_PARAM(math::Matrix4x4, ViewProj)
		DECLARE_PARAM(math::Matrix4x4, InvViewProj)
		DECLARE_PARAM(math::Vector3, CameraPos)
		DECLARE_PARAM_VALUE(float, WorldThickness, 0.06f)
		DECLARE_PARAM_VALUE(int32_t, NumRays, 16)
		DECLARE_PARAM_VALUE(int32_t, FrameIndex, 0) // 用于时间维度随机种子
		DECLARE_PARAM(math::Vector2, Resolution)
		DECLARE_PARAM_VALUE(float, TemporalBlendFactor, 0.85f)
		DECLARE_PARAM(math::Vector3, Pad0)
	BEGIN_STRUCT_CONSTRUCT(SSRContants)
		END_STRUCT_CONSTRUCT
	END_SHADER_STRUCT

	struct SSRProcessorPrivate
	{
		SSRProcessorPrivate(DynamicRHI* InRHI)
			:RHI(InRHI)
			, GET_SHADER_STRUCT_MEMBER(SSRContants)(InRHI)
		{

		}

		DynamicRHI* RHI = nullptr;
		std::shared_ptr<RHIPixelShader> SSRShader;
		std::shared_ptr<RHIVertexShader> VertexShader;
		std::shared_ptr<RHITexture2D> SSRBuffer;
		std::shared_ptr<RHITexture2D> SSRHistoryBuffer[2];
		uint32_t FrameIndexMod2 = 0;
		bool First = true;
		DECLARE_SHADER_STRUCT_MEMBER(SSRContants);
	};

	SSRProcessor::SSRProcessor(DynamicRHI* RHI)
		:d_ptr(new SSRProcessorPrivate(RHI))
	{
	}

	SSRProcessor::~SSRProcessor()
	{
		delete d_ptr;
	}

	void SSRProcessor::InitResource()
	{
		C_P(SSRProcessor);
		std::wstring ShaderPath = core::process_directory().wstring() + L"/ShaderLibDX/";
		std::wstring SSRShaderPath = ShaderPath + L"SSR.hlsl";
		d->SSRShader = d->RHI->RHICreatePixelShader(SSRShaderPath, "PS_SSR", {});
		ShaderPath += L"PostProcess.hlsl";
		d->VertexShader = d->RHI->RHICreateVertexShader(ShaderPath, "VS_ScreenQuad", {}, {});
	}

	void SSRProcessor::Draw(RHICommandContext& RHIContext, std::shared_ptr<GBuffer> TargetBuffer,std::shared_ptr<RHIViewPort> ViewPort,
							std::shared_ptr<RHITexture2D> HistorySceneColor,
		                    std::shared_ptr<CameraComponent> Camera)
	{
		C_P(SSRProcessor);
		RenderCore::RHICommandMark Mark(RHIContext, "SSR");

		d->FrameIndexMod2 = Camera->GetFrameIndexMod2();
		uint32_t Dst = d->FrameIndexMod2 ^ 1;

		if (!d->SSRBuffer)
		{
			d->SSRBuffer = d->RHI->RHICreateTexture2D(EPixelFormat::PF_FloatRGBA,
				ETextureCreateFlags::TexCreate_RenderTargetable | ETextureCreateFlags::TexCreate_ShaderResource,
				ViewPort->GetSize().cx, ViewPort->GetSize().cy, 1);
		}

		if (!d->SSRHistoryBuffer[0])
		{
			d->SSRHistoryBuffer[0] = d->RHI->RHICreateTexture2D(EPixelFormat::PF_FloatRGBA,
				ETextureCreateFlags::TexCreate_RenderTargetable | ETextureCreateFlags::TexCreate_ShaderResource,
				ViewPort->GetSize().cx, ViewPort->GetSize().cy, 1);
		}
		if (!d->SSRHistoryBuffer[1])
		{
			d->SSRHistoryBuffer[1] = d->RHI->RHICreateTexture2D(EPixelFormat::PF_FloatRGBA,
				ETextureCreateFlags::TexCreate_RenderTargetable | ETextureCreateFlags::TexCreate_ShaderResource,
				ViewPort->GetSize().cx, ViewPort->GetSize().cy, 1);
		}

		RHIContext.SetViewPort(0, 0, ViewPort->GetSize().cx, ViewPort->GetSize().cy);
		RHIContext.Clear(d->SSRBuffer, nullptr, core::FLinearColor::Transparent, 1.f, 0);
		RenderCore::GraphicsPipelineStateInitializer Init;
		Init.VertexShader = d->VertexShader;
		Init.PixelShader = d->SSRShader;

		Init.BlendState = RenderCore::RHICachedStates::BlendTraditional;
		Init.DepthStencilState = RenderCore::RHICachedStates::DepthStateDisable;
		Init.RasterizerState = RenderCore::RHICachedStates::RasterizerStateCullNone;

		RHIContext.RHISetGraphicsPipelineState(Init);
		RHIContext.SetRenderTarget(d->SSRBuffer, nullptr);

		RHIContext.RHISetShaderSampler(RenderCore::SF_Pixel, 0, RenderCore::RHICachedStates::ClampLinerSampler);
		RHIContext.RHISetShaderSampler(RenderCore::SF_Pixel, 1, RenderCore::RHICachedStates::ClampPointSampler);
		RHIContext.RHISetShaderTexture(RenderCore::SF_Pixel, 0, TargetBuffer->GetNormalBuffer());
		RHIContext.RHISetShaderTexture(RenderCore::SF_Pixel, 1, TargetBuffer->GetMetallicRoughnessBuffer());
		RHIContext.RHISetShaderTexture(RenderCore::SF_Pixel, 2, TargetBuffer->GetDepth());
		RHIContext.RHISetShaderTexture(RenderCore::SF_Pixel, 3, HistorySceneColor);
		RHIContext.RHISetShaderTexture(RenderCore::SF_Pixel, 4, TargetBuffer->GetMotionVector());
		RHIContext.RHISetShaderTexture(RenderCore::SF_Pixel, 5, d->SSRHistoryBuffer[d->FrameIndexMod2] ? d->SSRHistoryBuffer[d->FrameIndexMod2] : d->SSRBuffer);
		
		d->GET_UNIFORMDATA(SSRContants).ViewProj = Camera->GetViewMatrix() * Camera->GetProjMatrix();
		d->GET_UNIFORMDATA(SSRContants).InvViewProj = d->GET_UNIFORMDATA(SSRContants).ViewProj.Inverse();
		d->GET_UNIFORMDATA(SSRContants).CameraPos = Camera->GetCameraPos();
		d->GET_UNIFORMDATA(SSRContants).NumRays = 1;
		d->GET_UNIFORMDATA(SSRContants).FrameIndex = d->First ? 1 : Camera->GetFrameIndex();
		d->GET_UNIFORMDATA(SSRContants).Resolution = math::Vector2(static_cast<float>(ViewPort->GetSize().cx), static_cast<float>(ViewPort->GetSize().cy));
		d->First = false;
		
		d->GET_SHADER_STRUCT_MEMBER(SSRContants).UpdateUniformBuffer();
		d->GET_SHADER_STRUCT_MEMBER(SSRContants).SetShaderUniformBuffer(RenderCore::EShaderFrequency::SF_Pixel);
		RHIContext.Draw(3);

		if (d->SSRHistoryBuffer[Dst])
		{
			RHIContext.RHICopyResource(d->SSRHistoryBuffer[Dst], d->SSRBuffer);
		}
	}

	std::shared_ptr<RenderCore::RHITexture2D> SSRProcessor::GetSSRBuffer() const
	{
		C_P(SSRProcessor);
		return d->SSRBuffer;
	}

}