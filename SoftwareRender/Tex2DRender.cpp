#include "Tex2DRender.h"
#include "Render/IBLRender.h"
#include "core/system.h"
#include "Engine/Engine.h"
#include "Engine/Render/SceneRender.h"
#include "Imgui/imgui.h"
#include "core/strings.h"
#include "RHI/RHIPipeLineState.h"
#include "RHI/RHICachedStates.h"
#include "RHI/DynamicRHI.h"
#include "RHI/RHIViewPort.h"
#include "RHI/RHITextureCube.h"
#include "App/AppWindow.h"
#include "Render/CubeRender.h"

using namespace math;
using namespace RenderCore;

Tex2DRender::Tex2DRender(RenderCore::DynamicRHI* RHI)
:m_RHI(RHI)
, GET_SHADER_STRUCT_MEMBER(PSRenderDemoContant)(RHI)
, GET_SHADER_STRUCT_MEMBER(CBPerFrame)(RHI)
, GET_SHADER_STRUCT_MEMBER(CBPerObject)(RHI)
{
}

Tex2DRender::~Tex2DRender()
{

}

void Tex2DRender::InitResource()
{
	m_ViewPort = Engine::GEngine->GetSceneRender()->GetViewPort();

	std::wstring ShaderPath = core::process_directory().wstring() + L"/ShaderLibDX/";
	ShaderPath += L"EnvironmentShaders.hlsl";
	m_ShowTexture2DVS = m_RHI->RHICreateVertexShader(ShaderPath, "VS_ShowTexture2D", {}, {});
	m_ShowTexture2DPS = m_RHI->RHICreatePixelShader(ShaderPath, "PS_ShowTexture2DNormal", {});

}

void Tex2DRender::InitTex(const core::vec2u& texSize, const uint8_t* buffer)
{
	m_tex2D = m_RHI->RHICreateTexture2D(EPixelFormat::PF_B8G8R8A8, ETextureCreateFlags::TexCreate_ShaderResource, texSize.cx, texSize.cy, 1, (void*)buffer, texSize.w * 4);
}

void Tex2DRender::Draw(RenderCore::RHICommandContext& RHIContext, std::shared_ptr<RenderCore::RHIViewPort> ViewPort, float DeltaTime)
{
	ShowTexture2D(RHIContext, m_tex2D, ViewPort);
}


void Tex2DRender::ShowTexture2D(RenderCore::RHICommandContext& RHIContext, std::shared_ptr<RenderCore::RHITexture2D> Texture2D, std::shared_ptr<RenderCore::RHIViewPort> ViewPort)
{
	if (!Texture2D)
		return;
	std::shared_ptr<Engine::AppWindow> AppWin = Engine::GEngine->GetAppWindow();
	float AspectRatio = Texture2D->GetSize().w * 1.f / Texture2D->GetSize().h;
	int Width = std::min(AppWin->GetWidth(), Texture2D->GetSize().w);
	int Height = std::min(AppWin->GetHeight(), Texture2D->GetSize().h);
	Width = std::min(Width, static_cast<int>(Height * AspectRatio));
	Height = std::min(Height, static_cast<int>(Width / AspectRatio));
	RHIContext.SetViewPort((AppWin->GetWidth() - Width) / 2, (AppWin->GetHeight() - Height) / 2, Width, Height);

	RenderCore::GraphicsPipelineStateInitializer Init;
	Init.VertexShader = m_ShowTexture2DVS;
	Init.PixelShader = m_ShowTexture2DPS;
	Init.BlendState = RenderCore::RHICachedStates::BlendDisable;
	Init.DepthStencilState = RenderCore::RHICachedStates::DepthStateDisable;
	Init.RasterizerState = RenderCore::RHICachedStates::RasterizerStateCullNone;
	RHIContext.RHISetGraphicsPipelineState(Init);
	ViewPort->SetRenderTarget();

	RHIContext.RHISetShaderSampler(RenderCore::SF_Pixel, 0, RenderCore::RHICachedStates::ClampPointSampler);
	RHIContext.RHISetShaderTexture(RenderCore::SF_Pixel, 0, Texture2D);
	GET_UNIFORMDATA(PSRenderDemoContant).Exposure = m_Exposure;
	GET_SHADER_STRUCT_MEMBER(PSRenderDemoContant).UpdateUniformBuffer();
	GET_SHADER_STRUCT_MEMBER(PSRenderDemoContant).SetShaderUniformBuffer(RenderCore::EShaderFrequency::SF_Pixel);
	RHIContext.Draw(3);
}
