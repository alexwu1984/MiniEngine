#include "Render/SkyLightRenderPass.h"
#include "math/matrix4x4.h"
#include "RHI/RHIShaderDefine.h"
#include "math/vector3.h"
#include "RHI/RHIShdader.h"
#include "RHI/RHIRenderPass.h"
#include "RHI/RHITexture2D.h"
#include "Render/RDGUtils.h"
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
		float SunDirX = 0.f;
		float SunDirY = 0.f;
		float SunDirZ = 0.f;
		float SunBloomLinearHDR = 0.f;
		float HemiSkyGroundBlendPower = 1.75f;
		float GroundLatLongIntensity = 1.f;
		int32_t GroundLatLongEnabled = 0;
		int32_t PadSkyCb = 0;
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
									const math::Matrix4x4& SkyInverseViewProj,
									const math::Vector3& SunTowardSourceWorld,
									float SunBloomLinearHDR,
									std::shared_ptr<RenderCore::RHITexture2D> GroundLatLongOrDummy,
									float HemiSkyGroundBlendPower,
									float GroundLatLongIntensity,
									int32_t GroundLatLongEnabled)
	{
		C_P(SkyLightRenderPass);
		if (!d->TexCube)
			return;
		RenderCore::RHICommandMark Mark(RHIContext, "SkyLightRenderPass");

		RenderCore::FRHIRenderPassDesc Om = RenderCore::FRHIRenderPassDesc::ColorTargetsAndDepth(Targets, Depth);
		Om.DebugName = "SkyLightRenderPass_RasterOM";
		{
			FRDGPassDescriptor B{};
			using A = RenderCore::FRDGResourceAccess;
			if (GroundLatLongOrDummy)
				B.Inputs.push_back({ "GroundLatLong", [GroundLatLongOrDummy]() { return GroundLatLongOrDummy; }, true, A::SRV });
			for (const std::shared_ptr<RHITexture2D>& T : Targets)
			{
				if (!T)
					continue;
				auto TexCopy = T;
				B.Outputs.push_back({ "SkyRT", [TexCopy]() { return TexCopy; }, true, A::RTV });
			}
			if (Depth)
			{
				auto DepthCopy = Depth;
				B.Outputs.push_back({ "Depth", [DepthCopy]() { return DepthCopy; }, true, A::DSV });
			}
			FRDGUtils::AppendPassTextureBarriers(B, Om.DeclaredTextureBarriers);
			if (d->TexCube)
			{
				FRDGTextureBarrierDesc CubeSrv{};
				CubeSrv.TextureCube = d->TexCube;
				CubeSrv.Access = FRDGResourceAccess::SRV;
				Om.DeclaredTextureBarriers.push_back(std::move(CubeSrv));
			}
		}
		RenderCore::FRHIRenderPassScope RasterScope(RHIContext, std::move(Om));

		GraphicsPipelineStateInitializer Init;
		Init.VertexShader = d->VertexShader;
		Init.PixelShader = d->PixelShader;

		Init.BlendState = RHICachedStates::BlendOnAlphaOff;
		Init.DepthStencilState = RHICachedStates::DepthStateDisable;
		Init.RasterizerState = RHICachedStates::RasterizerStateCullNone;

		RHIContext.RHISetGraphicsPipelineState(Init);
		RHIContext.RHISetShaderSampler(RenderCore::SF_Pixel, 0, RHICachedStates::ClampLinerSampler);

		auto& UB = d->GET_UNIFORMDATA(CBSkyLightRenderPass);
		UB.InvViewProj = SkyInverseViewProj;
		UB.SunDirX = SunTowardSourceWorld.x;
		UB.SunDirY = SunTowardSourceWorld.y;
		UB.SunDirZ = SunTowardSourceWorld.z;
		UB.SunBloomLinearHDR = SunBloomLinearHDR;
		UB.HemiSkyGroundBlendPower = HemiSkyGroundBlendPower;
		UB.GroundLatLongIntensity = GroundLatLongIntensity;
		UB.GroundLatLongEnabled = GroundLatLongEnabled;
		UB.PadSkyCb = 0;
		RenderCore::RHI_UpdateAndBindUniformBufferVSPS(RHIContext, d->GET_SHADER_STRUCT_MEMBER(CBSkyLightRenderPass));

		RHIContext.RHISetShaderTexture(RenderCore::SF_Pixel, 0, d->TexCube);
		if (GroundLatLongOrDummy)
			RHIContext.RHISetShaderTexture(RenderCore::SF_Pixel, 1, GroundLatLongOrDummy);
		RHIContext.Draw(3);
	}

	void SkyLightRenderPass::SetTextureCube(std::shared_ptr<RenderCore::RHITextureCube> TexCube)
	{
		C_P(SkyLightRenderPass);
		d->TexCube = TexCube;
	}

} // namespace Engine
