#include "LiquidClassDemo.h"
#include "core/system.h"
#include "RHI/RHIShdader.h"
#include "RHI/RHIShaderDefine.h"
#include "RHI/RHIPipeLineState.h"
#include "RHI/RHICachedStates.h"
#include "RHI/DynamicRHI.h"
#include "RHI/RHIViewPort.h"
#include "RHI/RHIRenderTarget.h"
#include "Engine/Engine.h"
#include "Render/GBuffer.h"
#include "Render/RenderUtil.h"
#include "App/AppWindow.h"
#include "Engine/Engine.h"
#include "Engine/Render/SceneRender.h"
#include "Imgui/imgui.h"
#include "Render/Blur.h"
#include "math/vector4.h"


void SceneCamera::SetOrthographic(float size, float nearClip, float farClip)
{
	m_OrthographicSize = size;
	m_OrthographicNearClip = nearClip;
	m_OrthographicFarClip = farClip;
	RecalculateProjection();
}

void SceneCamera::SetViewportSize(uint32_t width, uint32_t height)
{
	m_AspectRatio = (float)width / (float)height;
	RecalculateProjection();
}

void SceneCamera::RecalculateProjection()
{
	float orthoLeft = -m_OrthographicSize * m_AspectRatio * 0.5f;
	float orthoRight = m_OrthographicSize * m_AspectRatio * 0.5f;
	float orthoBottom = -m_OrthographicSize * 0.5f;
	float orthoTop = m_OrthographicSize * 0.5f;

	m_Projection = math::Matrix4x4::MatrixOrthographicOffCenterLH(orthoLeft, orthoRight,
		orthoBottom, orthoTop, m_OrthographicNearClip, m_OrthographicFarClip);
}

LiquidClassDemo::LiquidClassDemo(RenderCore::DynamicRHI* RHI)
	:m_RHI(RHI)
	, GET_SHADER_STRUCT_MEMBER(PSRenderDemoContant)(RHI)
	, GET_SHADER_STRUCT_MEMBER(LuquidClassContant)(RHI)
{

}

LiquidClassDemo::~LiquidClassDemo()
{

}

void LiquidClassDemo::InitResource()
{
	m_ViewPort = Engine::GEngine->GetSceneRender()->GetViewPort();
	std::wstring ShaderPath = core::process_directory().wstring() + L"/ShaderLibDX/";
	ShaderPath += L"EnvironmentShaders.hlsl";
	m_ShowTexture2DVS = m_RHI->RHICreateVertexShader(ShaderPath, "VS_ShowTexture2D", {}, {});
	m_ShowTexture2DPS = m_RHI->RHICreatePixelShader(ShaderPath, "PS_ShowTexture2DNormal", {});

	ShaderPath = core::process_directory().wstring() + L"/ShaderLibDX/";
	ShaderPath += L"LuquidClass.hlsl";
	m_LinquidClassVS = m_RHI->RHICreateVertexShader(ShaderPath, "VS_ScreenQuad", {}, {});
	m_LinquidClassPS = m_RHI->RHICreatePixelShader(ShaderPath, "PS_LiquidGlass", {});

	std::wstring TexPath = core::process_directory().wstring() + L"/GLTFModel/";
	m_BackgroundTex = m_RHI->RHICreateTexture2D(TexPath + L"anime.png");
	m_BlurPs = std::make_shared<Engine::BlurPS>(m_RHI);
	m_BlurPs->InitResource(0);

	m_LuquidSrcTex = m_RHI->RHICreateTexture2D(m_BackgroundTex->GetPixelFormat(), RenderCore::TexCreate_ShaderResource, 500, 500, 1);
	m_LuquidClassRT = m_RHI->RHICreateRenderTarget(m_BackgroundTex->GetPixelFormat(), 500, 500, 1, false, false);

	m_Camera.SetOrthographic(15.f, -10.0f, 10.0f);
	m_Camera.SetViewportSize(m_BackgroundTex->GetSize().cx, m_BackgroundTex->GetSize().cy);
}

void LiquidClassDemo::Draw(RenderCore::RHICommandContext& RHIContext, std::shared_ptr<RenderCore::RHIViewPort> ViewPort, float DeltaTime)
{
	if (m_BackgroundTex && m_LuquidSrcTex)
	{
		RHIContext.RHICopyResource2D(m_LuquidSrcTex, m_BackgroundTex, core::vec4u(80, 200, 500, 500));

		m_WorldPos = GetWorldPos(core::vec2f(80, 200), core::vec2f(m_BackgroundTex->GetSize().cx, m_BackgroundTex->GetSize().cy));

		math::Vector3 a_Postion[4]{ math::Vector3(-1, -1, 0.0),
									math::Vector3(1, -1, 0.0),
									math::Vector3(-1, 1, 0.0),
									math::Vector3(1, 1, 0.0) };
		//math::Matrix4x4 WorldTranslate = math::Matrix4x4::ScaleMatrix(math::Vector3(5, 5, 1)) * math::Matrix4x4::CreateFromTranslate(m_WorldPos.x, m_WorldPos.y,0);
		//math::Matrix4x4 Mat = WorldTranslate * m_Camera.GetProjection();
		//for(int32_t index = 0; index < 4; ++index)
		//	a_Postion[index] = Mat.TransformPosition(a_Postion[index]);

		math::Vector3 u_midPoint = (a_Postion[0] + a_Postion[3]) * 0.5f;
		math::Vector3 u_quadNDC2ScreenNDCScale = (a_Postion[3] - a_Postion[0]) * 0.5f;
		GET_UNIFORMDATA(LuquidClassContant).u_quadNDC2ScreenNDCScale = math::Vector2(u_quadNDC2ScreenNDCScale.x, u_quadNDC2ScreenNDCScale.y);
		GET_UNIFORMDATA(LuquidClassContant).u_midPoint = u_midPoint;
		m_BlurPs->Draw(RHIContext, m_LuquidSrcTex);
		LuquidClass(RHIContext, m_BlurPs->GetResult());
		//ShowTexture2D(RHIContext, m_BlurPs->GetResult());
		ShowTexture2D(RHIContext, m_LuquidClassRT->GetTex());
	}
}

core::vec2f LiquidClassDemo::GetWorldPos(const core::vec2f& PicPos, const core::vec2f& ScreenSize)
{
	core::vec2f ScreenPos = core::vec2f(2.f) * (PicPos / ScreenSize) - core::vec2f(1.f);
	//ScreenPos.y *= -1.0f;
	math::Matrix4x4 VPMatrix = m_Camera.GetProjection();
	math::Matrix4x4 VPMatrixInverse = VPMatrix.Inverse();
	math::Vector3 PosInWorld = VPMatrixInverse.TranslateVector(math::Vector3(ScreenPos.x,ScreenPos.y,1.0));
	return core::vec2f(PosInWorld.x, PosInWorld.y);
}

void LiquidClassDemo::ShowTexture2D(RenderCore::RHICommandContext& RHIContext, std::shared_ptr<RenderCore::RHITexture2D> Tex2D)
{
	if (!Tex2D || !m_ViewPort)
		return;

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
	m_ViewPort->SetRenderTarget();
		
	RHIContext.RHISetShaderSampler(RenderCore::SF_Pixel, 0, RenderCore::RHICachedStates::ClampPointSampler);
	RHIContext.RHISetShaderTexture(RenderCore::SF_Pixel, 0, Tex2D);
	GET_UNIFORMDATA(PSRenderDemoContant).Exposure = m_Exposure;
	GET_SHADER_STRUCT_MEMBER(PSRenderDemoContant).UpdateUniformBuffer();
	GET_SHADER_STRUCT_MEMBER(PSRenderDemoContant).SetShaderUniformBuffer(RenderCore::EShaderFrequency::SF_Pixel);
	RHIContext.Draw(3);
}

void LiquidClassDemo::LuquidClass(RenderCore::RHICommandContext& RHIContext, std::shared_ptr<RenderCore::RHITexture2D> Tex2D)
{
	
	RHIContext.SetViewPort(0, 0, m_LuquidClassRT->GetSize().x, m_LuquidClassRT->GetSize().y);

	RenderCore::GraphicsPipelineStateInitializer Init;
	Init.VertexShader = m_LinquidClassVS;
	Init.PixelShader = m_LinquidClassPS;
	Init.BlendState = RenderCore::RHICachedStates::BlendDisable;
	Init.DepthStencilState = RenderCore::RHICachedStates::DepthStateDisable;
	Init.RasterizerState = RenderCore::RHICachedStates::RasterizerStateCullNone;
	RHIContext.RHISetGraphicsPipelineState(Init);
	RHIContext.SetRenderTarget(m_LuquidClassRT);

	RHIContext.RHISetShaderSampler(RenderCore::SF_Pixel, 0, RenderCore::RHICachedStates::ClampLinerSampler);
	RHIContext.RHISetShaderTexture(RenderCore::SF_Pixel, 0, Tex2D);
	GET_SHADER_STRUCT_MEMBER(LuquidClassContant).UpdateUniformBuffer();
	GET_SHADER_STRUCT_MEMBER(LuquidClassContant).SetShaderUniformBuffer(RenderCore::EShaderFrequency::SF_Pixel);
	RHIContext.Draw(3);
}

