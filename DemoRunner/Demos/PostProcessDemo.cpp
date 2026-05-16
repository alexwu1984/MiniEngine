#include "DemoRunner/Demos/PostProcessDemo.h"

#include "core/system.h"
#include "core/logger.h"
#include "RHI/RHIPipeLineState.h"
#include "RHI/RHICachedStates.h"
#include "RHI/DynamicRHI.h"
#include "RHI/RHIShaderDefine.h"
#include "RHI/RHIViewPort.h"
#include "RHI/RHIRenderPass.h"
#include "Render/RDGUtils.h"

PostProcessorDemo::PostProcessorDemo(RenderCore::DynamicRHI* InRHI)
	: RHI(InRHI)
	, GET_SHADER_STRUCT_MEMBER(cbTransition1)(InRHI)
{
}

PostProcessorDemo::~PostProcessorDemo() = default;

void PostProcessorDemo::InitResource()
{
	std::wstring shaderPath = core::process_directory().wstring() + L"/ShaderLibDX/PostProcessDemo.hlsl";
	VertexShader = RHI->RHICreateVertexShader(shaderPath, "VS_ScreenQuad", {}, {});
	PixelShader = RHI->RHICreatePixelShader(shaderPath, "PS_Transition1", {});

	const std::wstring texPath = core::process_directory().wstring() + L"/GLTFModel/";
	Texture1 = RHI->RHICreateTexture2D(texPath + L"tifa_wallpaper_3840x2160.png");
	Texture2 = RHI->RHICreateTexture2D(texPath + L"aerith_wallpaper_3840x2160.jpg");
	if (!Texture1 || !Texture2)
	{
		core::logger::err() << "PostProcessDemo: texture load failed (expect "
							 << core::ucs2_u8(texPath + L"tifa_wallpaper_3840x2160.png") << " and "
							 << core::ucs2_u8(texPath + L"aerith_wallpaper_3840x2160.jpg")
							 << ") — Draw skipped; clear color is black.";
	}
}

void PostProcessorDemo::Draw(RenderCore::RHICommandContext& Ctx,
							 std::shared_ptr<RenderCore::RHIViewPort> ViewPort,
							 float DeltaTime)
{
	if (!Texture1 || !Texture2 || !ViewPort)
		return;

	std::shared_ptr<RenderCore::RHITexture2D> BackBuf = ViewPort->GetBackBuffer();
	if (!BackBuf)
		return;

	RenderCore::FRHIRenderPassDesc Om = RenderCore::FRHIRenderPassDesc::SingleColorNoDepth(BackBuf);
	Om.DebugName = "PostProcessDemo";
	Engine::FRDGUtils::AppendFullscreenDeclaredTextureBarriers(Om, {{"Tex1", Texture1}, {"Tex2", Texture2}}, BackBuf);
	RenderCore::FRHIRenderPassScope DemoScope(Ctx, std::move(Om));

	RenderCore::GraphicsPipelineStateInitializer Init;
	Init.VertexShader = VertexShader;
	Init.PixelShader = PixelShader;
	Init.BlendState = RenderCore::RHICachedStates::BlendTraditional;
	Init.DepthStencilState = RenderCore::RHICachedStates::DepthStateDisable;
	Init.RasterizerState = RenderCore::RHICachedStates::RasterizerStateCullNone;

	Ctx.RHISetGraphicsPipelineState(Init);

	Ctx.RHISetShaderSampler(RenderCore::SF_Pixel, 0, RenderCore::RHICachedStates::WarpLinerSampler);
	Ctx.RHISetShaderTexture(RenderCore::SF_Pixel, 0, Texture1);
	Ctx.RHISetShaderTexture(RenderCore::SF_Pixel, 1, Texture2);

	GET_UNIFORMDATA(cbTransition1).progress += DeltaTime * 0.1f;
	if (GET_UNIFORMDATA(cbTransition1).progress >= 1.0f)
		GET_UNIFORMDATA(cbTransition1).progress = 0.0f;

	RenderCore::RHI_UpdateAndBindUniformBuffer(Ctx, GET_SHADER_STRUCT_MEMBER(cbTransition1), RenderCore::SF_Pixel);
	Ctx.Draw(3);
}

