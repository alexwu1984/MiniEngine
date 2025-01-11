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
#include "App/AppWindow.h"

IBLRenderDemo::IBLRenderDemo(RenderCore::DynamicRHI* RHI)
:m_RHI(RHI)
, GET_SHADER_STRUCT_MEMBER(PSRenderDemoContant)(RHI)
{
	m_IBLRender = std::make_shared<Engine::IBLRender>(RHI);
}

IBLRenderDemo::~IBLRenderDemo()
{

}

void IBLRenderDemo::InitResource()
{
	if (m_IBLRender)
		m_IBLRender->InitResource();
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
	RenderCore::RHIVertexDeclare VertexDeclareRHI;
	VertexDeclareRHI.AppendDeclareInput(RenderCore::VertexDeclareInput(0, RenderCore::EVertexElementType::VET_Float3, false));
	m_SkyVS = m_RHI->RHICreateVertexShader(ShaderPath, "VS_SkyCube", VertexDeclareRHI, {});
	m_SkyPS = m_RHI->RHICreatePixelShader(ShaderPath, "PS_SkyCube", {});
	m_GenIrradiancePS = m_RHI->RHICreatePixelShader(ShaderPath, "PS_GenIrradiance", {});
	m_GenPrefilterPS = m_RHI->RHICreatePixelShader(ShaderPath, "PS_GenPrefiltered", {});

	Engine::GEngine->GetSceneRender()->sigGuiEvent.bind([this] {

		static bool ShowConfig = true;
		if (!ShowConfig)
			return;

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
		}
		ImGui::End();

		GET_UNIFORMDATA(PSRenderDemoContant).Exposure = m_Exposure;
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
	ShowTexture2D(RHIContext);
}

void IBLRenderDemo::GenerateIBLMaps()
{
	if (m_CurrentHDR != m_ChooseHDR)
	{
		m_CurrentHDR = m_ChooseHDR;
		m_IBLRender->LoadTex(core::u8_ucs2(m_AllHDRFiles[m_ChooseHDR]));
	}
}

void IBLRenderDemo::ShowTexture2D(RenderCore::RHICommandContext& RHIContext)
{
	if (!m_IBLRender->GetHDRTex())
		return;
	std::shared_ptr<Engine::AppWindow> AppWin = Engine::GEngine->GetAppWindow();
	auto Texture2D = m_IBLRender->GetHDRTex();
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
	RHIContext.RHISetShaderTexture(RenderCore::SF_Pixel, 0, m_IBLRender->GetHDRTex());
	GET_SHADER_STRUCT_MEMBER(PSRenderDemoContant).UpdateUniformBuffer();
	GET_SHADER_STRUCT_MEMBER(PSRenderDemoContant).SetShaderUniformBuffer(RenderCore::EShaderFrequency::SF_Pixel);
	RHIContext.Draw(3);
}
