#include "Render/SkyLightRenderPass.h"
#include "math/matrix4x4.h"
#include "RHI/RHIShaderDefine.h"
#include "RHI/RHIShdader.h"
#include "RHI/RHITexture2D.h"
#include "RHI/DynamicRHI.h"
#include "RHI/RHICachedStates.h"
#include "core/system.h"
#include "Engine/Engine.h"
#include "App/AppWindow.h"

using namespace RenderCore;

namespace Engine
{
	struct CBSkyLightRenderPass
	{
		math::Matrix4x4 InvViewProj{};
	};
	using CBSkyLightRenderPassWrap = RenderCore::TUniformBufferBinding<CBSkyLightRenderPass, 0u>;

	struct SkyLightRenderPassPrivate
	{
		RenderCore::DynamicRHI* RHI;
		std::shared_ptr<RHIVertexShader> VertexShader;
		std::shared_ptr<RHIPixelShader> PixelShader;
		std::shared_ptr<RHITextureCube> TexCube;

		SkyLightRenderPassPrivate(RenderCore::DynamicRHI* _RHI)
			:RHI(_RHI),
			GET_SHADER_STRUCT_MEMBER(CBSkyLightRenderPass)(_RHI)
		{
		}
		DECLARE_SHADER_STRUCT_MEMBER(CBSkyLightRenderPass);
	};

	SkyLightRenderPass::SkyLightRenderPass(RenderCore::DynamicRHI* RHI)
		:d_ptr(new SkyLightRenderPassPrivate(RHI))
	{
	}

	SkyLightRenderPass::~SkyLightRenderPass()
	{
		delete d_ptr;
	}

	void SkyLightRenderPass::InitResource()
	{
		C_P(SkyLightRenderPass);
		InitShader();
	}

	void SkyLightRenderPass::InitShader()
	{
		C_P(SkyLightRenderPass);
		std::wstring ShaderPath = core::process_directory().wstring() + L"/ShaderLibDX/";
		ShaderPath += L"SkyLightRenderPass.hlsl";

		d->VertexShader = d->RHI->RHICreateVertexShader(ShaderPath, "VS_SkyFullscreen", {}, {});
		d->PixelShader = d->RHI->RHICreatePixelShader(ShaderPath, "PS", {});
	}

	void SkyLightRenderPass::Render(RenderCore::RHICommandContext& RHIContext,
									const std::vector<std::shared_ptr<RenderCore::RHITexture2D>>& Targets,
									std::shared_ptr<RenderCore::RHITexture2D> Depth,
									const math::Matrix4x4& SkyInverseViewProj)
	{
		C_P(SkyLightRenderPass);
		if (!d->TexCube)
			return;
		RenderCore::RHICommandMark Mark(RHIContext, "SkyLightRenderPass");
		GraphicsPipelineStateInitializer Init;
		Init.VertexShader = d->VertexShader;
		Init.PixelShader = d->PixelShader;

		Init.BlendState = RHICachedStates::BlendOnAlphaOff;
		Init.DepthStencilState = RHICachedStates::DepthStateDisable;
		Init.RasterizerState = RHICachedStates::RasterizerStateCullNone;

		RHIContext.SetRenderTarget(Targets, Depth);
		int32_t w = 0;
		int32_t h = 0;
		if (!Targets.empty() && Targets.front())
		{
			const core::vec2i Sz = Targets.front()->GetSize();
			w = Sz.x;
			h = Sz.y;
		}
		if (w <= 0 || h <= 0)
		{
			w = GEngine->GetAppWindow()->GetWidth();
			h = GEngine->GetAppWindow()->GetHeight();
		}
		RHIContext.SetViewPort(0, 0, w, h);
		RHIContext.RHISetGraphicsPipelineState(Init);
		RHIContext.RHISetShaderSampler(RenderCore::SF_Pixel, 0, RHICachedStates::ClampLinerSampler);

		d->GET_UNIFORMDATA(CBSkyLightRenderPass).InvViewProj = SkyInverseViewProj;
		RenderCore::RHI_UpdateAndBindUniformBuffer(RHIContext, d->GET_SHADER_STRUCT_MEMBER(CBSkyLightRenderPass), RenderCore::SF_Vertex);

		RHIContext.RHISetShaderTexture(RenderCore::SF_Pixel, 0, d->TexCube);
		RHIContext.Draw(3);
	}

	void SkyLightRenderPass::SetTextureCube(std::shared_ptr<RenderCore::RHITextureCube> TexCube)
	{
		C_P(SkyLightRenderPass);
		d->TexCube = TexCube;
	}

} // namespace Engine
