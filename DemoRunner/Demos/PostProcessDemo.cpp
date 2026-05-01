#include "DemoRunner/Demos/PostProcessDemo.h"

#include "core/system.h"
#include "RHI/RHIPipeLineState.h"
#include "RHI/RHICachedStates.h"
#include "RHI/DynamicRHI.h"
#include "RHI/RHIShaderDefine.h"
#include "RHI/RHIViewPort.h"

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
}

void PostProcessorDemo::Draw(RenderCore::RHICommandContext& Ctx,
							 std::shared_ptr<RenderCore::RHIViewPort> ViewPort,
							 float DeltaTime)
{
	if (!Texture1 || !Texture2 || !ViewPort)
		return;

	RenderCore::GraphicsPipelineStateInitializer Init;
	Init.VertexShader = VertexShader;
	Init.PixelShader = PixelShader;
	Init.BlendState = RenderCore::RHICachedStates::BlendTraditional;
	Init.DepthStencilState = RenderCore::RHICachedStates::DepthStateDisable;
	Init.RasterizerState = RenderCore::RHICachedStates::RasterizerStateCullNone;

	Ctx.RHISetGraphicsPipelineState(Init);
	ViewPort->SetRenderTarget();

	Ctx.RHISetShaderSampler(RenderCore::SF_Pixel, 0, RenderCore::RHICachedStates::WarpLinerSampler);
	Ctx.RHISetShaderTexture(RenderCore::SF_Pixel, 0, Texture1);
	Ctx.RHISetShaderTexture(RenderCore::SF_Pixel, 1, Texture2);

	GET_UNIFORMDATA(cbTransition1).progress += DeltaTime * 0.1f;
	if (GET_UNIFORMDATA(cbTransition1).progress >= 1.0f)
		GET_UNIFORMDATA(cbTransition1).progress = 0.0f;

	RenderCore::RHI_UpdateAndBindUniformBuffer(Ctx, GET_SHADER_STRUCT_MEMBER(cbTransition1), RenderCore::SF_Pixel);
	Ctx.Draw(3);
}

