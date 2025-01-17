#include "IBLRenderDemo.h"
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

IBLRenderDemo::IBLRenderDemo(RenderCore::DynamicRHI* RHI)
:m_RHI(RHI)
, GET_SHADER_STRUCT_MEMBER(PSRenderDemoContant)(RHI)
, GET_SHADER_STRUCT_MEMBER(CBPerFrame)(RHI)
, GET_SHADER_STRUCT_MEMBER(CBPerObject)(RHI)
{
	m_IBLRender = std::make_shared<Engine::IBLRender>(RHI);
	m_CubeMapCrossRender = std::make_shared<Engine::CubeMapCrossRender>(RHI);
}

IBLRenderDemo::~IBLRenderDemo()
{

}

void IBLRenderDemo::InitResource()
{
	if (m_IBLRender)
		m_IBLRender->InitResource();
	if (m_CubeMapCrossRender)
		m_CubeMapCrossRender->InitResource();
	m_ViewPort = Engine::GEngine->GetSceneRender()->GetViewPort();

	std::wstring HdrFile = core::process_directory().wstring() + L"/GLTFModel/";
	for (const auto& entry : std::filesystem::directory_iterator(HdrFile))
	{
		std::string path = entry.path().string();
		if (path.rfind(".hdr") != std::string::npos)
		{
			size_t lastPeriodIndex = path.find_last_of('\\');
			if (lastPeriodIndex == std::string::npos)
			{
				lastPeriodIndex = path.find_last_of('/');
			}
			Assert(lastPeriodIndex != std::string::npos);
			m_AllHDRFiles.push_back(path);
		}
	}

	std::wstring ShaderPath = core::process_directory().wstring() + L"/ShaderLibDX/";
	ShaderPath += L"EnvironmentShaders.hlsl";
	m_ShowTexture2DVS = m_RHI->RHICreateVertexShader(ShaderPath, "VS_ShowTexture2D", {}, {});
	m_ShowTexture2DPS = m_RHI->RHICreatePixelShader(ShaderPath, "PS_ShowTexture2D", {});
	{
		RenderCore::RHIVertexDeclare VertexDeclareRHI;
		VertexDeclareRHI.AppendDeclareInput(RenderCore::VertexDeclareInput(0, RenderCore::EVertexElementType::VET_Float3, false));
		VertexDeclareRHI.AppendDeclareInput(RenderCore::VertexDeclareInput(1, RenderCore::EVertexElementType::VET_Float3, false));
		m_CubeMapCrossVS = m_RHI->RHICreateVertexShader(ShaderPath, "VS_CubeMapCross", VertexDeclareRHI, {});
	}
	m_CubeMapCrossPS = m_RHI->RHICreatePixelShader(ShaderPath, "PS_CubeMapCross", {});
	m_GenIrradiancePS = m_RHI->RHICreatePixelShader(ShaderPath, "PS_GenIrradiance", {});
	m_GenPrefilterPS = m_RHI->RHICreatePixelShader(ShaderPath, "PS_GenPrefiltered", {});

	Engine::GEngine->GetSceneRender()->sigGuiEvent.bind([this] {

		static bool ShowConfig = true;
		if (!ShowConfig)
			return;
		ImGui::SetNextWindowPos(ImVec2(1, 1));
		if (ImGui::Begin("Config", &ShowConfig, ImGuiWindowFlags_AlwaysAutoResize))
		{
			ImGui::ColorEdit3("Clear Color", &m_ClearColor.x);
			ImGui::SliderFloat("Exposure", &m_Exposure, 0.f, 10.f, "%.1f");

			ImGui::Separator();

			ImGui::BeginGroup();
			ImGui::Text("HDR files");
			ImGui::Indent(20);
			for (int i = 0; i < m_AllHDRFiles.size(); ++i)
			{
				std::filesystem::path Path = m_AllHDRFiles[i];
				std::string fileName = Path.filename().string();
				ImGui::RadioButton(fileName.c_str(), &m_ChooseHDR, i);
			}
			ImGui::EndGroup();

			ImGui::BeginGroup();
			ImGui::Text("Show Mode");
			ImGui::Indent(20);
			ImGui::RadioButton("Long-Lat View", &m_ShowMode, SM_LongLat);
			ImGui::RadioButton("Cube Cross", &m_ShowMode, SM_CubeMapCross);
			ImGui::RadioButton("Irradiance", &m_ShowMode, SM_Irradiance);
			ImGui::RadioButton("Prefiltered", &m_ShowMode, SM_Prefiltered);
			ImGui::RadioButton("PreintegratedGF", &m_ShowMode, SM_PreintegratedGF);
			ImGui::EndGroup();

			if (m_ShowMode == SM_CubeMapCross)
			{
				ImGui::SliderInt("Mip Level", &m_MipLevel, 0, m_IBLRender->GetEvnCube()->GetNumMips() - 1);
			}
			else if (m_ShowMode == SM_Irradiance)
			{
				ImGui::SliderInt("Mip Level", &m_MipLevel, 0, m_IBLRender->GetIrrCube()->GetNumMips() - 1);
			}
			else if (m_ShowMode == SM_Prefiltered)
			{
				ImGui::SliderInt("Mip Level", &m_MipLevel, 0, m_IBLRender->GetPreFilterCube()->GetNumMips() - 1);
			}
		}
		ImGui::End();

		Engine::GEngine->GetSceneRender()->SetBackgroundColor(m_ClearColor);
		
		}, this);
}

void IBLRenderDemo::Draw(RenderCore::RHICommandContext& RHIContext, std::shared_ptr<RenderCore::RHIViewPort> ViewPort, float DeltaTime)
{
	if (!m_IBLRender)
		return;
	GenerateIBLMaps();
	m_IBLRender->Draw(RHIContext);
	if (m_ViewPort)
		m_ViewPort->SetRenderTarget();
	switch (m_ShowMode)
	{
	case SM_LongLat:
		ShowTexture2D(RHIContext, m_IBLRender->GetHDRTex());
		break;
	case SM_CubeMapCross:
		ShowSHCubeMapDebugView(RHIContext, m_IBLRender->GetEvnCube());
		break;
	case SM_Irradiance:
		ShowSHCubeMapDebugView(RHIContext, m_IBLRender->GetIrrCube());
		break;
	case SM_Prefiltered:
		ShowSHCubeMapDebugView(RHIContext, m_IBLRender->GetPreFilterCube());
		break;
	case SM_PreintegratedGF:
		ShowTexture2D(RHIContext, m_IBLRender->GetPreIntegrateBRDF());
		break;
	}
}

void IBLRenderDemo::GenerateIBLMaps()
{
	if (m_CurrentHDR != m_ChooseHDR)
	{
		m_CurrentHDR = m_ChooseHDR;
		m_IBLRender->LoadTex(core::u8_ucs2(m_AllHDRFiles[m_ChooseHDR]));
	}
}

void IBLRenderDemo::ShowTexture2D(RenderCore::RHICommandContext& RHIContext, std::shared_ptr<RenderCore::RHITexture2D> Texture2D)
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

	RHIContext.RHISetShaderSampler(RenderCore::SF_Pixel, 0, RenderCore::RHICachedStates::ClampPointSampler);
	RHIContext.RHISetShaderTexture(RenderCore::SF_Pixel, 0, Texture2D);
	GET_UNIFORMDATA(PSRenderDemoContant).Exposure = m_Exposure;
	GET_SHADER_STRUCT_MEMBER(PSRenderDemoContant).UpdateUniformBuffer();
	GET_SHADER_STRUCT_MEMBER(PSRenderDemoContant).SetShaderUniformBuffer(RenderCore::EShaderFrequency::SF_Pixel);
	RHIContext.Draw(3);
}

void IBLRenderDemo::ShowSHCubeMapDebugView(RenderCore::RHICommandContext& RHIContext, std::shared_ptr<RenderCore::RHITextureCube> Cube)
{
	if (!m_CubeMapCrossRender)
		return;
	std::shared_ptr<Engine::AppWindow> AppWin = Engine::GEngine->GetAppWindow();
	uint32_t Size = std::min(AppWin->GetWidth(), AppWin->GetHeight());
	RHIContext.SetViewPort((AppWin->GetWidth() - Size) / 2, (AppWin->GetHeight() - Size) / 2, Size, Size);

	RenderCore::GraphicsPipelineStateInitializer Init;
	Init.VertexShader = m_CubeMapCrossVS;
	Init.PixelShader = m_CubeMapCrossPS;
	Init.BlendState = RenderCore::RHICachedStates::BlendDisable;
	Init.DepthStencilState = RenderCore::RHICachedStates::DepthStateDisable;
	Init.RasterizerState = RenderCore::RHICachedStates::RasterizerStateCullNone;
	RHIContext.RHISetGraphicsPipelineState(Init);

	GET_UNIFORMDATA(CBPerObject).myPerObject_u_mCurrWorld = Matrix4x4();
	GET_SHADER_STRUCT_MEMBER(CBPerObject).UpdateUniformBuffer();
	GET_SHADER_STRUCT_MEMBER(CBPerObject).SetShaderUniformBuffer(RenderCore::SF_Vertex);

	GET_UNIFORMDATA(CBPerFrame).myPerFrame.CameraCurrViewProj = Matrix4x4::MatrixOrthoLH(1.f, 1.f, -1.f, 1.f);
	GET_SHADER_STRUCT_MEMBER(CBPerFrame).UpdateUniformBuffer();
	GET_SHADER_STRUCT_MEMBER(CBPerFrame).SetShaderUniformBuffer(RenderCore::SF_Vertex);
	GET_UNIFORMDATA(PSRenderDemoContant).MipLevel = m_MipLevel;
	GET_UNIFORMDATA(PSRenderDemoContant).Exposure = m_Exposure;
	GET_SHADER_STRUCT_MEMBER(PSRenderDemoContant).UpdateUniformBuffer();
	GET_SHADER_STRUCT_MEMBER(PSRenderDemoContant).SetShaderUniformBuffer(RenderCore::EShaderFrequency::SF_Pixel);

	RHIContext.RHISetShaderSampler(RenderCore::SF_Pixel, 0, RenderCore::RHICachedStates::ClampPointSampler);
	RHIContext.RHISetShaderTexture(RenderCore::SF_Pixel, 0, Cube);
	m_CubeMapCrossRender->Render(RHIContext);
}
