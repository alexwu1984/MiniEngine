#include "DemoRunner/Demos/LiquidClassDemoRunner.h"

#include "core/system.h"

#include "App/AppWindow.h"
#include "RHI/DynamicRHI.h"
#include "RHI/RHIViewPort.h"
#include "RHI/RHIRenderTarget.h"
#include "RHI/RHIPipeLineState.h"
#include "RHI/RHICachedStates.h"

#include "Render/Blur.h"

#include "Imgui/imgui.h"

using namespace DemoRunner;

void SceneCamera::SetOrthographic(float size, float nearClip, float farClip)
{
	OrthoSize = size;
	OrthoNear = nearClip;
	OrthoFar = farClip;
	RecalculateProjection();
}

void SceneCamera::SetViewportSize(uint32_t width, uint32_t height)
{
	Aspect = (float)width / (float)height;
	RecalculateProjection();
}

void SceneCamera::RecalculateProjection()
{
	const float l = -OrthoSize * Aspect * 0.5f;
	const float r = OrthoSize * Aspect * 0.5f;
	const float b = -OrthoSize * 0.5f;
	const float t = OrthoSize * 0.5f;
	Projection = math::Matrix4x4::MatrixOrthographicOffCenterLH(l, r, b, t, OrthoNear, OrthoFar);
}

LiquidClassDemoRunner::LiquidClassDemoRunner(RenderCore::DynamicRHI* InRHI)
	: RHI(InRHI)
	, GET_SHADER_STRUCT_MEMBER(PSRenderDemoContant)(InRHI)
	, GET_SHADER_STRUCT_MEMBER(LuquidClassContant)(InRHI)
{
}

LiquidClassDemoRunner::~LiquidClassDemoRunner() = default;

void LiquidClassDemoRunner::Init(RenderCore::DynamicRHI* InRHI,
								 const std::shared_ptr<RenderCore::RHIViewPort>& InViewPort,
								 const std::shared_ptr<Engine::AppWindow>& InWindow)
{
	RHI = InRHI;
	ViewPort = InViewPort;
	Window = InWindow;

	std::wstring shaderPath = core::process_directory().wstring() + L"/ShaderLibDX/EnvironmentShaders.hlsl";
	ShowTexture2DVS = RHI->RHICreateVertexShader(shaderPath, "VS_ShowTexture2D", {}, {});
	ShowTexture2DPS = RHI->RHICreatePixelShader(shaderPath, "PS_ShowTexture2DNormal", {});

	shaderPath = core::process_directory().wstring() + L"/ShaderLibDX/LuquidClass.hlsl";
	LiquidVS = RHI->RHICreateVertexShader(shaderPath, "VS_ScreenQuad", {}, {});
	LiquidPS = RHI->RHICreatePixelShader(shaderPath, "PS_LiquidGlass", {});

	const std::wstring texPath = core::process_directory().wstring() + L"/GLTFModel/anime.png";
	BackgroundTex = RHI->RHICreateTexture2D(texPath);

	Blur = std::make_shared<Engine::BlurPS>(RHI);
	Blur->InitResource(0);

	// Fixed sized working set for debugging (no resize churn).
	LiquidSrcTex = RHI->RHICreateTexture2D(BackgroundTex->GetPixelFormat(), RenderCore::TexCreate_ShaderResource, 500, 500, 1);
	LiquidRT = RHI->RHICreateRenderTarget(BackgroundTex->GetPixelFormat(), 500, 500, 1, false, false);

	Camera.SetOrthographic(15.f, -10.0f, 10.0f);
	Camera.SetViewportSize(BackgroundTex->GetSize().cx, BackgroundTex->GetSize().cy);
}

void LiquidClassDemoRunner::OnGui()
{
	ImGui::Text("LiquidClassDemo");
	ImGui::SliderFloat("Exposure", &Exposure, 0.f, 10.f, "%.2f");
	ImGui::Separator();
	ImGui::Text("Source: GLTFModel/anime.png");
}

void LiquidClassDemoRunner::Draw(RenderCore::RHICommandContext& Ctx,
								 const std::shared_ptr<RenderCore::RHIViewPort>&,
								 float)
{
	if (!BackgroundTex || !LiquidSrcTex || !Blur || !LiquidRT)
		return;

	Ctx.RHICopyResource2D(LiquidSrcTex, BackgroundTex, core::vec4u(80, 200, 500, 500));

	[[maybe_unused]] const core::vec2f worldPos = GetWorldPos(core::vec2f(80, 200), core::vec2f(BackgroundTex->GetSize().cx, BackgroundTex->GetSize().cy));

	math::Vector3 quad[4] = {
		math::Vector3(-1, -1, 0.0f),
		math::Vector3( 1, -1, 0.0f),
		math::Vector3(-1,  1, 0.0f),
		math::Vector3( 1,  1, 0.0f),
	};

	const math::Vector3 mid = (quad[0] + quad[3]) * 0.5f;
	const math::Vector3 scale = (quad[3] - quad[0]) * 0.5f;
	GET_UNIFORMDATA(LuquidClassContant).u_quadNDC2ScreenNDCScale = math::Vector2(scale.x, scale.y);
	GET_UNIFORMDATA(LuquidClassContant).u_midPoint = mid;

	Blur->Draw(Ctx, LiquidSrcTex);
	LiquidClass(Ctx, Blur->GetResult());
	ShowTexture2D(Ctx, LiquidRT->GetTex());
}

core::vec2f LiquidClassDemoRunner::GetWorldPos(const core::vec2f& PicPos, const core::vec2f& ScreenSize)
{
	const core::vec2f screenPos = core::vec2f(2.f) * (PicPos / ScreenSize) - core::vec2f(1.f);
	const math::Matrix4x4 VP = Camera.GetProjection();
	const math::Matrix4x4 inv = VP.Inverse();
	const math::Vector3 posW = inv.TranslateVector(math::Vector3(screenPos.x, screenPos.y, 1.0));
	return core::vec2f(posW.x, posW.y);
}

void LiquidClassDemoRunner::ShowTexture2D(RenderCore::RHICommandContext& Ctx, const std::shared_ptr<RenderCore::RHITexture2D>& Tex2D)
{
	if (!Tex2D || !ViewPort || !Window)
		return;

	const float aspect = Tex2D->GetSize().w * 1.f / Tex2D->GetSize().h;
	int W = std::min(Window->GetWidth(), Tex2D->GetSize().w);
	int H = std::min(Window->GetHeight(), Tex2D->GetSize().h);
	W = std::min(W, (int)(H * aspect));
	H = std::min(H, (int)(W / aspect));
	Ctx.SetViewPort((Window->GetWidth() - W) / 2, (Window->GetHeight() - H) / 2, W, H);

	RenderCore::GraphicsPipelineStateInitializer Init;
	Init.VertexShader = ShowTexture2DVS;
	Init.PixelShader = ShowTexture2DPS;
	Init.BlendState = RenderCore::RHICachedStates::BlendDisable;
	Init.DepthStencilState = RenderCore::RHICachedStates::DepthStateDisable;
	Init.RasterizerState = RenderCore::RHICachedStates::RasterizerStateCullNone;
	Ctx.RHISetGraphicsPipelineState(Init);

	ViewPort->SetRenderTarget();

	Ctx.RHISetShaderSampler(RenderCore::SF_Pixel, 0, RenderCore::RHICachedStates::ClampPointSampler);
	Ctx.RHISetShaderTexture(RenderCore::SF_Pixel, 0, Tex2D);
	GET_UNIFORMDATA(PSRenderDemoContant).Exposure = Exposure;
	GET_SHADER_STRUCT_MEMBER(PSRenderDemoContant).UpdateUniformBuffer();
	GET_SHADER_STRUCT_MEMBER(PSRenderDemoContant).SetShaderUniformBuffer(RenderCore::EShaderFrequency::SF_Pixel);
	Ctx.Draw(3);
}

void LiquidClassDemoRunner::LiquidClass(RenderCore::RHICommandContext& Ctx, const std::shared_ptr<RenderCore::RHITexture2D>& Tex2D)
{
	if (!LiquidRT || !Tex2D)
		return;

	Ctx.SetViewPort(0, 0, LiquidRT->GetSize().x, LiquidRT->GetSize().y);

	RenderCore::GraphicsPipelineStateInitializer Init;
	Init.VertexShader = LiquidVS;
	Init.PixelShader = LiquidPS;
	Init.BlendState = RenderCore::RHICachedStates::BlendDisable;
	Init.DepthStencilState = RenderCore::RHICachedStates::DepthStateDisable;
	Init.RasterizerState = RenderCore::RHICachedStates::RasterizerStateCullNone;
	Ctx.RHISetGraphicsPipelineState(Init);

	Ctx.SetRenderTarget(LiquidRT);

	Ctx.RHISetShaderSampler(RenderCore::SF_Pixel, 0, RenderCore::RHICachedStates::ClampLinerSampler);
	Ctx.RHISetShaderTexture(RenderCore::SF_Pixel, 0, Tex2D);
	GET_SHADER_STRUCT_MEMBER(LuquidClassContant).UpdateUniformBuffer();
	GET_SHADER_STRUCT_MEMBER(LuquidClassContant).SetShaderUniformBuffer(RenderCore::EShaderFrequency::SF_Pixel);
	Ctx.Draw(3);
}

