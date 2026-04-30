#include "DemoRunner/Demos/IBLRenderDemoRunner.h"

#include "Render/IBLRender.h"
#include "Render/CubeRender.h"
#include "core/system.h"

#include "App/AppWindow.h"
#include "RHI/DynamicRHI.h"
#include "RHI/RHIViewPort.h"
#include "RHI/RHITextureCube.h"
#include "RHI/RHIPipeLineState.h"
#include "RHI/RHICachedStates.h"

#include "Imgui/imgui.h"

using namespace DemoRunner;

IBLRenderDemoRunner::IBLRenderDemoRunner(RenderCore::DynamicRHI* InRHI)
	: RHI(InRHI)
	, GET_SHADER_STRUCT_MEMBER(PSRenderDemoContant)(InRHI)
	, GET_SHADER_STRUCT_MEMBER(CBPerFrame)(InRHI)
	, GET_SHADER_STRUCT_MEMBER(CBPerObject)(InRHI)
{
}

IBLRenderDemoRunner::~IBLRenderDemoRunner() = default;

void IBLRenderDemoRunner::Init(RenderCore::DynamicRHI* InRHI,
							   const std::shared_ptr<RenderCore::RHIViewPort>& InViewPort,
							   const std::shared_ptr<Engine::AppWindow>& InWindow)
{
	RHI = InRHI;
	ViewPort = InViewPort;
	Window = InWindow;

	IBL = std::make_shared<Engine::FSkyLightIBLPrecompute>(RHI);
	CubeCross = std::make_shared<Engine::CubeMapCrossRender>(RHI);
	if (IBL) IBL->InitResource();
	if (CubeCross) CubeCross->InitResource();

	// Enumerate HDRs.
	const std::wstring hdrDir = core::process_directory().wstring() + L"/GLTFModel/HDR/";
	for (const auto& entry : std::filesystem::directory_iterator(hdrDir))
	{
		const std::string p = entry.path().string();
		if (p.rfind(".hdr") != std::string::npos)
			AllHDRFiles.push_back(p);
	}

	std::wstring shaderPath = core::process_directory().wstring() + L"/ShaderLibDX/EnvironmentShaders.hlsl";
	ShowTexture2DVS = RHI->RHICreateVertexShader(shaderPath, "VS_ShowTexture2D", {}, {});
	ShowTexture2DPS = RHI->RHICreatePixelShader(shaderPath, "PS_ShowTexture2D", {});

	{
		RenderCore::RHIVertexDeclare vd;
		vd.AppendDeclareInput(RenderCore::VertexDeclareInput(0, RenderCore::EVertexElementType::VET_Float3, false));
		vd.AppendDeclareInput(RenderCore::VertexDeclareInput(1, RenderCore::EVertexElementType::VET_Float3, false));
		CubeCrossVS = RHI->RHICreateVertexShader(shaderPath, "VS_CubeMapCross", vd, {});
	}
	CubeCrossPS = RHI->RHICreatePixelShader(shaderPath, "PS_CubeMapCross", {});
}

void IBLRenderDemoRunner::OnGui()
{
	ImGui::Text("IBLRenderDemo");
	ImGui::ColorEdit3("Clear Color", &Clear.x);
	ImGui::SliderFloat("Exposure", &Exposure, 0.f, 10.f, "%.1f");

	ImGui::Separator();
	ImGui::Text("HDR files");
	ImGui::Indent(16);
	for (int i = 0; i < (int)AllHDRFiles.size(); ++i)
	{
		std::filesystem::path p = AllHDRFiles[i];
		const std::string fileName = p.filename().string();
		ImGui::RadioButton(fileName.c_str(), &ChooseHDR, i);
	}
	ImGui::Unindent(16);

	ImGui::Separator();
	ImGui::Text("Show Mode");
	ImGui::RadioButton("LongLat", &Mode, SM_LongLat);
	ImGui::RadioButton("CubeCross", &Mode, SM_CubeCross);
	ImGui::RadioButton("Irradiance", &Mode, SM_Irradiance);
	ImGui::RadioButton("Prefiltered", &Mode, SM_Prefiltered);
	ImGui::RadioButton("PreintegratedGF", &Mode, SM_PreintegratedGF);

	if (IBL)
	{
		if (Mode == SM_CubeCross && IBL->GetSkyLightCubemap())
			ImGui::SliderInt("Mip Level", &MipLevel, 0, IBL->GetSkyLightCubemap()->GetNumMips() - 1);
		else if (Mode == SM_Irradiance && IBL->GetDiffuseIrradianceCubemap())
			ImGui::SliderInt("Mip Level", &MipLevel, 0, IBL->GetDiffuseIrradianceCubemap()->GetNumMips() - 1);
		else if (Mode == SM_Prefiltered && IBL->GetSpecularReflectionCubemap())
			ImGui::SliderInt("Mip Level", &MipLevel, 0, IBL->GetSpecularReflectionCubemap()->GetNumMips() - 1);
	}
}

void IBLRenderDemoRunner::Draw(RenderCore::RHICommandContext& Ctx,
							   const std::shared_ptr<RenderCore::RHIViewPort>&,
							   float)
{
	if (!IBL || !Window)
		return;

	GenerateIBLMaps();
	IBL->Draw(Ctx);

	switch (Mode)
	{
	case SM_LongLat:
		ShowTexture2D(Ctx, IBL->GetSkyLightSourceHDR());
		break;
	case SM_CubeCross:
		ShowSHCubeMapDebugView(Ctx, IBL->GetSkyLightCubemap());
		break;
	case SM_Irradiance:
		ShowSHCubeMapDebugView(Ctx, IBL->GetDiffuseIrradianceCubemap());
		break;
	case SM_Prefiltered:
		ShowSHCubeMapDebugView(Ctx, IBL->GetSpecularReflectionCubemap());
		break;
	case SM_PreintegratedGF:
		ShowTexture2D(Ctx, IBL->GetBRDFIntegrationLUT());
		break;
	}
}

void IBLRenderDemoRunner::GenerateIBLMaps()
{
	if (!IBL)
		return;
	if (AllHDRFiles.empty())
		return;

	if (CurrentHDR != ChooseHDR)
	{
		CurrentHDR = ChooseHDR;
		CurrentHDR = std::max(0, std::min(CurrentHDR, (int)AllHDRFiles.size() - 1));
		IBL->LoadTex(core::u8_ucs2(AllHDRFiles[CurrentHDR]));
	}
}

void IBLRenderDemoRunner::ShowTexture2D(RenderCore::RHICommandContext& Ctx, const std::shared_ptr<RenderCore::RHITexture2D>& Texture2D)
{
	if (!Texture2D || !ViewPort || !Window)
		return;

	const float aspect = Texture2D->GetSize().w * 1.f / Texture2D->GetSize().h;
	int W = std::min(Window->GetWidth(), Texture2D->GetSize().w);
	int H = std::min(Window->GetHeight(), Texture2D->GetSize().h);
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
	Ctx.RHISetShaderTexture(RenderCore::SF_Pixel, 1, Texture2D);
	GET_UNIFORMDATA(PSRenderDemoContant).Exposure = Exposure;
	GET_UNIFORMDATA(PSRenderDemoContant).MipLevel = 0;
	GET_SHADER_STRUCT_MEMBER(PSRenderDemoContant).UpdateUniformBuffer();
	GET_SHADER_STRUCT_MEMBER(PSRenderDemoContant).SetShaderUniformBuffer(RenderCore::EShaderFrequency::SF_Pixel);
	Ctx.Draw(3);
}

void IBLRenderDemoRunner::ShowSHCubeMapDebugView(RenderCore::RHICommandContext& Ctx, const std::shared_ptr<RenderCore::RHITextureCube>& Cube)
{
	if (!CubeCross || !Cube || !ViewPort || !Window)
		return;

	const uint32_t size = (uint32_t)std::min(Window->GetWidth(), Window->GetHeight());
	Ctx.SetViewPort((Window->GetWidth() - (int)size) / 2, (Window->GetHeight() - (int)size) / 2, (int)size, (int)size);

	RenderCore::GraphicsPipelineStateInitializer Init;
	Init.VertexShader = CubeCrossVS;
	Init.PixelShader = CubeCrossPS;
	Init.BlendState = RenderCore::RHICachedStates::BlendDisable;
	Init.DepthStencilState = RenderCore::RHICachedStates::DepthStateDisable;
	Init.RasterizerState = RenderCore::RHICachedStates::RasterizerStateCullNone;
	Ctx.RHISetGraphicsPipelineState(Init);

	ViewPort->SetRenderTarget();

	GET_UNIFORMDATA(CBPerObject).myPerObject_u_mCurrWorld = math::Matrix4x4();
	GET_SHADER_STRUCT_MEMBER(CBPerObject).UpdateUniformBuffer();
	GET_SHADER_STRUCT_MEMBER(CBPerObject).SetShaderUniformBuffer(RenderCore::SF_Vertex);

	GET_UNIFORMDATA(CBPerFrame).myPerFrame.CameraCurrViewProj = math::Matrix4x4::MatrixOrthoLH(1.f, 1.f, -1.f, 1.f);
	GET_SHADER_STRUCT_MEMBER(CBPerFrame).UpdateUniformBuffer();
	GET_SHADER_STRUCT_MEMBER(CBPerFrame).SetShaderUniformBuffer(RenderCore::SF_Vertex);

	GET_UNIFORMDATA(PSRenderDemoContant).MipLevel = MipLevel;
	GET_UNIFORMDATA(PSRenderDemoContant).Exposure = Exposure;
	GET_SHADER_STRUCT_MEMBER(PSRenderDemoContant).UpdateUniformBuffer();
	GET_SHADER_STRUCT_MEMBER(PSRenderDemoContant).SetShaderUniformBuffer(RenderCore::EShaderFrequency::SF_Pixel);

	Ctx.RHISetShaderSampler(RenderCore::SF_Pixel, 0, RenderCore::RHICachedStates::ClampPointSampler);
	Ctx.RHISetShaderTexture(RenderCore::SF_Pixel, 0, Cube);

	CubeCross->Render(Ctx);
}

