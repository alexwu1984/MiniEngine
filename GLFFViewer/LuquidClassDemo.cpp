#include "LuquidClassDemo.h"
#include "core/system.h"
#include "RHI/RHIShdader.h"
#include "RHI/RHIShaderDefine.h"
#include "RHI/RHIPipeLineState.h"
#include "RHI/RHICachedStates.h"
#include "RHI/DynamicRHI.h"
#include "RHI/RHIViewPort.h"
#include "Engine/Engine.h"
#include "Render/GBuffer.h"
#include "Render/RenderUtil.h"
#include "App/AppWindow.h"
#include "Engine/Engine.h"
#include "Engine/Render/SceneRender.h"
#include "Imgui/imgui.h"
#include "Render/Blur.h"

LuquidClassDemo::LuquidClassDemo(RenderCore::DynamicRHI* RHI)
	:m_RHI(RHI)
	, GET_SHADER_STRUCT_MEMBER(PSRenderDemoContant)(RHI)
{

}

LuquidClassDemo::~LuquidClassDemo()
{

}

void LuquidClassDemo::InitResource()
{
	m_ViewPort = Engine::GEngine->GetSceneRender()->GetViewPort();
	std::wstring ShaderPath = core::process_directory().wstring() + L"/ShaderLibDX/";
	ShaderPath += L"EnvironmentShaders.hlsl";
	m_ShowTexture2DVS = m_RHI->RHICreateVertexShader(ShaderPath, "VS_ShowTexture2D", {}, {});
	m_ShowTexture2DPS = m_RHI->RHICreatePixelShader(ShaderPath, "PS_ShowTexture2D", {});

	std::wstring TexPath = core::process_directory().wstring() + L"/GLTFModel/";
	m_BackgroundTex = m_RHI->RHICreateTexture2D(TexPath + L"anime.png");
	m_BlurPs = std::make_shared<Engine::BlurPS>(m_RHI);
	m_BlurPs->InitResource(0);
}

void LuquidClassDemo::Draw(RenderCore::RHICommandContext& RHIContext, std::shared_ptr<RenderCore::RHIViewPort> ViewPort, float DeltaTime)
{
	if (m_BackgroundTex)
	{
		m_BlurPs->Draw(RHIContext, m_BackgroundTex);
		ShowTexture2D(RHIContext, m_BlurPs->GetResult());
	}
}

void LuquidClassDemo::ShowTexture2D(RenderCore::RHICommandContext& RHIContext, std::shared_ptr<RenderCore::RHITexture2D> Tex2D)
{
	if (!Tex2D)
		return;
	if (m_ViewPort)
		m_ViewPort->SetRenderTarget();
	std::shared_ptr<Engine::AppWindow> AppWin = Engine::GEngine->GetAppWindow();
	float AspectRatio = Tex2D->GetSize().w * 1.f / Tex2D->GetSize().h;
	int Width = std::min(AppWin->GetWidth(), Tex2D->GetSize().w);
	int Height = std::min(AppWin->GetHeight(), Tex2D->GetSize().h);
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

	RHIContext.RHISetShaderSampler(RenderCore::SF_Pixel, 0, RenderCore::RHICachedStates::ClampPointSampler);
	RHIContext.RHISetShaderTexture(RenderCore::SF_Pixel, 0, Tex2D);
	GET_UNIFORMDATA(PSRenderDemoContant).Exposure = m_Exposure;
	GET_SHADER_STRUCT_MEMBER(PSRenderDemoContant).UpdateUniformBuffer();
	GET_SHADER_STRUCT_MEMBER(PSRenderDemoContant).SetShaderUniformBuffer(RenderCore::EShaderFrequency::SF_Pixel);
	RHIContext.Draw(3);
}
