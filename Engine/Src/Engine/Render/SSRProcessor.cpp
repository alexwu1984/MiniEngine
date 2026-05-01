#include "Render/SSRProcessor.h"
#include "RHI/RHIShdader.h"
#include "RHI/RHIShaderDefine.h"
#include "RHI/RHIPipeLineState.h"
#include "RHI/RHICachedStates.h"
#include "RHI/DynamicRHI.h"
#include "RHI/RHIDefinitions.h"
#include "RHI/RHITexture2D.h"
#include "RHI/RHIViewPort.h"
#include "Render/SceneRendering/SceneViewData.h"
#include "core/system.h"
#include "math/vector2.h"
#include "Render/SceneTextures.h"
#include "Render/RenderTexturePool.h"

using namespace RenderCore;

namespace Engine
{
	struct SSRContants
	{
		math::Matrix4x4 ViewProj{};
		math::Matrix4x4 InvViewProj{};
		math::Vector3 CameraPos{};
		float WorldThickness{ 0.06f };
		int32_t NumRays{ 16 };
		int32_t FrameIndex{ 0 };
		math::Vector2 Resolution{};
		float TemporalBlendFactor{ 0.93f };
		math::Vector3 Pad0{};
	};
	using SSRContantsWrap = RenderCore::TUniformBufferBinding<SSRContants, 0u>;

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
		std::shared_ptr<RHITexture2D> SSRHistoryBuffer[2];
		uint32_t FrameIndexMod2 = 0;
		bool First = true;
		uint32_t LastTemporalHistoryGeneration = ~0u;
		DECLARE_SHADER_STRUCT_MEMBER(SSRContants);
	};

	SSRProcessor::SSRProcessor(DynamicRHI* RHI)
		:d_ptr(new SSRProcessorPrivate(RHI))
	{
	}

	SSRProcessor::~SSRProcessor()
	{
		InvalidateTransientResources();
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

	void SSRProcessor::InvalidateTransientResources()
	{
		C_P(SSRProcessor);
		const int32_t Fl = (int32_t)(ETextureCreateFlags::TexCreate_RenderTargetable | ETextureCreateFlags::TexCreate_ShaderResource);
		for (auto& T : d->SSRHistoryBuffer)
		{
			if (!T)
				continue;
			auto Sz = T->GetSize();
			RenderTexturePool::Get().ReleaseTexture2D(EPixelFormat::PF_FloatRGBA, Fl, Sz.x, Sz.y, 1, std::move(T));
		}
		d->First = true;
	}

	namespace
	{
		void EnsureSSRHistories(SSRProcessorPrivate* d, int32_t W, int32_t H)
		{
			const int32_t Fl = (int32_t)(ETextureCreateFlags::TexCreate_RenderTargetable | ETextureCreateFlags::TexCreate_ShaderResource);
			auto EnsureOne = [&](std::shared_ptr<RHITexture2D>& T) {
				if (T)
				{
					auto Sz = T->GetSize();
					if (Sz.x == W && Sz.y == H)
						return;
					RenderTexturePool::Get().ReleaseTexture2D(EPixelFormat::PF_FloatRGBA, Fl, Sz.x, Sz.y, 1, std::move(T));
				}
				T = RenderTexturePool::Get().AcquireTexture2D(d->RHI, EPixelFormat::PF_FloatRGBA, Fl, W, H, 1);
			};
			EnsureOne(d->SSRHistoryBuffer[0]);
			EnsureOne(d->SSRHistoryBuffer[1]);
		}
	}

	void SSRProcessor::Draw(RHICommandContext& RHIContext, std::shared_ptr<SceneTextures> TargetBuffer,std::shared_ptr<RHIViewPort> ViewPort,
							std::shared_ptr<RHITexture2D> HistorySceneColor,
		                    std::shared_ptr<const FSceneViewData> ViewData)
	{
		C_P(SSRProcessor);
		if (!ViewData)
			return;
		RenderCore::RHICommandMark Mark(RHIContext, "SSR");

		d->FrameIndexMod2 = static_cast<uint32_t>(ViewData->FrameIndexMod2);
		uint32_t Dst = d->FrameIndexMod2 ^ 1;

		const uint32_t camGen = ViewData->TemporalHistoryGeneration;
		if (camGen != d->LastTemporalHistoryGeneration)
		{
			d->LastTemporalHistoryGeneration = camGen;
			d->First = true;
		}

		const int32_t VpW = ViewPort->GetSize().cx;
		const int32_t VpH = ViewPort->GetSize().cy;
		EnsureSSRHistories(d, VpW, VpH);

		std::shared_ptr<RHITexture2D> WriteSSR = d->SSRHistoryBuffer[Dst];
		if (!WriteSSR)
			return;

		RHIContext.Clear(WriteSSR, nullptr, core::FLinearColor::Transparent, 1.f, 0);
		RenderCore::GraphicsPipelineStateInitializer Init;
		Init.VertexShader = d->VertexShader;
		Init.PixelShader = d->SSRShader;

		Init.BlendState = RenderCore::RHICachedStates::BlendTraditional;
		Init.DepthStencilState = RenderCore::RHICachedStates::DepthStateDisable;
		Init.RasterizerState = RenderCore::RHICachedStates::RasterizerStateCullNone;

		RHIContext.RHISetGraphicsPipelineState(Init);
		RHIContext.SetRenderTarget(WriteSSR, nullptr);
		RHIContext.SetViewPort(0, 0, VpW, VpH);

		RHIContext.RHISetShaderSampler(RenderCore::SF_Pixel, 0, RenderCore::RHICachedStates::ClampLinerSampler);
		RHIContext.RHISetShaderSampler(RenderCore::SF_Pixel, 1, RenderCore::RHICachedStates::ClampPointSampler);
		RHIContext.RHISetShaderTexture(RenderCore::SF_Pixel, 0, TargetBuffer->GetNormalBuffer());
		RHIContext.RHISetShaderTexture(RenderCore::SF_Pixel, 1, TargetBuffer->GetMetallicRoughnessBuffer());
		RHIContext.RHISetShaderTexture(RenderCore::SF_Pixel, 2, TargetBuffer->GetDepth());
		RHIContext.RHISetShaderTexture(RenderCore::SF_Pixel, 3, HistorySceneColor);
		RHIContext.RHISetShaderTexture(RenderCore::SF_Pixel, 4, TargetBuffer->GetMotionVector());
		{
			std::shared_ptr<RHITexture2D> PrevSSR = d->SSRHistoryBuffer[d->FrameIndexMod2];
			if (!PrevSSR)
				PrevSSR = WriteSSR;
			RHIContext.RHISetShaderTexture(RenderCore::SF_Pixel, 5, PrevSSR);
		}
		
		d->GET_UNIFORMDATA(SSRContants).ViewProj = ViewData->SsrViewProjMatrix;
		d->GET_UNIFORMDATA(SSRContants).InvViewProj = ViewData->SsrInvViewProjMatrix;
		d->GET_UNIFORMDATA(SSRContants).CameraPos = ViewData->CameraPos;
		d->GET_UNIFORMDATA(SSRContants).NumRays = 1;
		d->GET_UNIFORMDATA(SSRContants).FrameIndex = d->First ? 1 : ViewData->FrameIndex;
		d->GET_UNIFORMDATA(SSRContants).Resolution = math::Vector2(static_cast<float>(ViewPort->GetSize().cx), static_cast<float>(ViewPort->GetSize().cy));
		d->GET_UNIFORMDATA(SSRContants).TemporalBlendFactor = 0.93f;
		d->First = false;
		
		RHI_UpdateAndBindUniformBuffer(RHIContext, d->GET_SHADER_STRUCT_MEMBER(SSRContants), SF_Pixel);
		RHIContext.Draw(3);
	}

	std::shared_ptr<RenderCore::RHITexture2D> SSRProcessor::GetSSRBuffer() const
	{
		C_P(SSRProcessor);
		return d->SSRHistoryBuffer[d->FrameIndexMod2 ^ 1];
	}

}