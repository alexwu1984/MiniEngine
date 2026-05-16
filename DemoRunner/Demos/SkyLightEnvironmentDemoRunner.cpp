#include "DemoRunner/Demos/SkyLightEnvironmentDemoRunner.h"

#include "RHI/RHIShaderDefine.h"
#include "Engine/Render/SkyLightEnvironment.h"
#include "Engine/Render/CubeRender.h"
#include "core/system.h"

#include "App/AppWindow.h"
#include "RHI/DynamicRHI.h"
#include "RHI/RHIViewPort.h"
#include "RHI/RHIRenderPass.h"
#include "RHI/RHITextureCube.h"
#include "RHI/RHIPipeLineState.h"
#include "RHI/RHICachedStates.h"

#include "Imgui/imgui.h"

using namespace DemoRunner;

SkyLightEnvironmentDemoRunner::SkyLightEnvironmentDemoRunner(RenderCore::DynamicRHI* InRHI)
	: RHI(InRHI)
	, GET_SHADER_STRUCT_MEMBER(PSRenderDemoContant)(InRHI)
	, GET_SHADER_STRUCT_MEMBER(CBPerFrame)(InRHI)
	, GET_SHADER_STRUCT_MEMBER(CBPerObject)(InRHI)
{
}

SkyLightEnvironmentDemoRunner::~SkyLightEnvironmentDemoRunner() = default;

void SkyLightEnvironmentDemoRunner::Init(RenderCore::DynamicRHI* InRHI,
							   const std::shared_ptr<RenderCore::RHIViewPort>& InViewPort,
							   const std::shared_ptr<Engine::AppWindow>& InWindow)
{
	RHI = InRHI;
	ViewPort = InViewPort;
	Window = InWindow;

	SkyLightEnv = std::make_shared<Engine::USkyLightComponent>(RHI);
	CubeCross = std::make_shared<Engine::CubeMapCrossRender>(RHI);
	if (SkyLightEnv) SkyLightEnv->InitResource();
	if (CubeCross) CubeCross->InitResource();

	// Enumerate HDRs.
	const std::wstring hdrDir = core::process_directory().wstring() + L"/GLTFModel/HDR/";
	for (const auto& entry : std::filesystem::directory_iterator(hdrDir))
	{
		const std::string p = entry.path().string();
		if (p.rfind(".hdr") != std::string::npos)
			AllHDRFiles.push_back(p);
	}

	if (!AllHDRFiles.empty())
		ChooseEnvironment = 1;

	std::wstring shaderPath = core::process_directory().wstring() + L"/ShaderLibDX/EnvironmentShaders.hlsl";
	ShowTexture2DVS = RHI->RHICreateVertexShader(shaderPath, "VS_ShowTexture2D", {}, {});
	ShowTexture2DPS = RHI->RHICreatePixelShader(shaderPath, "PS_ShowTexture2D", {});
	ShowCubeEquirectPS = RHI->RHICreatePixelShader(shaderPath, "PS_ShowCubeEquirect", {});

	{
		RenderCore::RHIVertexDeclare vd;
		vd.AppendDeclareInput(RenderCore::VertexDeclareInput(0, RenderCore::EVertexElementType::VET_Float3, false));
		vd.AppendDeclareInput(RenderCore::VertexDeclareInput(1, RenderCore::EVertexElementType::VET_Float3, false));
		CubeCrossVS = RHI->RHICreateVertexShader(shaderPath, "VS_CubeMapCross", vd, {});
	}
	CubeCrossPS = RHI->RHICreatePixelShader(shaderPath, "PS_CubeMapCross", {});
}

void SkyLightEnvironmentDemoRunner::OnGui()
{
	ImGui::Text("SkyLight Environment (IBL precompute)");
	ImGui::ColorEdit3("Clear Color", &Clear.x);
	ImGui::SliderFloat("Exposure", &Exposure, 0.f, 10.f, "%.1f");

	ImGui::Separator();
	ImGui::Text("Environment source");
	ImGui::Indent(16);
	ImGui::RadioButton("Procedural sky (generated cubemap / IBL)", &ChooseEnvironment, 0);
	for (int i = 0; i < (int)AllHDRFiles.size(); ++i)
	{
		std::filesystem::path p = AllHDRFiles[i];
		const std::string fileName = p.filename().string();
		ImGui::RadioButton(fileName.c_str(), &ChooseEnvironment, i + 1);
	}
	ImGui::Unindent(16);
	if (ChooseEnvironment == 0)
		ImGui::SliderFloat3("Sun dir (toward light)", &ProceduralSunDirection.x, -1.f, 1.f);

	ImGui::Separator();
	ImGui::Text("Show Mode");
	ImGui::RadioButton("LongLat", &Mode, SM_LongLat);
	ImGui::RadioButton("CubeCross", &Mode, SM_CubeCross);
	ImGui::RadioButton("Irradiance", &Mode, SM_Irradiance);
	ImGui::RadioButton("Prefiltered", &Mode, SM_Prefiltered);
	ImGui::RadioButton("PreintegratedGF", &Mode, SM_PreintegratedGF);

	if (SkyLightEnv)
	{
		if (Mode == SM_LongLat && ChooseEnvironment == 0 && SkyLightEnv->GetSkyLightCubemap())
			ImGui::SliderInt("Mip Level", &MipLevel, 0, SkyLightEnv->GetSkyLightCubemap()->GetNumMips() - 1);
		else if (Mode == SM_CubeCross && SkyLightEnv->GetSkyLightCubemap())
			ImGui::SliderInt("Mip Level", &MipLevel, 0, SkyLightEnv->GetSkyLightCubemap()->GetNumMips() - 1);
		else if (Mode == SM_Irradiance && SkyLightEnv->GetDiffuseIrradianceCubemap())
			ImGui::SliderInt("Mip Level", &MipLevel, 0, SkyLightEnv->GetDiffuseIrradianceCubemap()->GetNumMips() - 1);
		else if (Mode == SM_Prefiltered && SkyLightEnv->GetSpecularReflectionCubemap())
			ImGui::SliderInt("Mip Level", &MipLevel, 0, SkyLightEnv->GetSpecularReflectionCubemap()->GetNumMips() - 1);
	}
}

void SkyLightEnvironmentDemoRunner::Draw(RenderCore::RHICommandContext& Ctx,
							   const std::shared_ptr<RenderCore::RHIViewPort>&,
							   float)
{
	if (!SkyLightEnv || !Window)
		return;

	GenerateIBLMaps();
	SkyLightEnv->Draw(Ctx);

	switch (Mode)
	{
	case SM_LongLat:
		if (auto hdr = SkyLightEnv->GetSkyLightSourceHDR())
			ShowTexture2D(Ctx, hdr);
		else if (auto cube = SkyLightEnv->GetSkyLightCubemap())
			ShowCubeAsLongLat(Ctx, cube);
		break;
	case SM_CubeCross:
		ShowSHCubeMapDebugView(Ctx, SkyLightEnv->GetSkyLightCubemap());
		break;
	case SM_Irradiance:
		ShowSHCubeMapDebugView(Ctx, SkyLightEnv->GetDiffuseIrradianceCubemap());
		break;
	case SM_Prefiltered:
		ShowSHCubeMapDebugView(Ctx, SkyLightEnv->GetSpecularReflectionCubemap());
		break;
	case SM_PreintegratedGF:
		ShowTexture2D(Ctx, SkyLightEnv->GetBRDFIntegrationLUT());
		break;
	}
}

void SkyLightEnvironmentDemoRunner::GenerateIBLMaps()
{
	if (!SkyLightEnv)
		return;

	const bool bProcedural = (ChooseEnvironment == 0);
	math::Vector3 sun = ProceduralSunDirection;
	if (sun.GetSqrLength() < 1e-10f)
		sun = math::Vector3(1.f, 0.05f, 0.f);
	const bool sunDirty =
		bProcedural && (sun - AppliedProceduralSun).GetSqrLength() > 1e-10f;
	const bool envDirty = (ChooseEnvironment != AppliedEnvironment) || sunDirty;
	if (!envDirty)
		return;

	if (bProcedural)
	{
		Engine::FSkyLightSourceDesc Desc{};
		Desc.Type = Engine::ESkyLightSourceType::Procedural;
		Desc.ProceduralSunDirectionTowardSource = sun;
		SkyLightEnv->InvalidateCapturedEnvironment();
		SkyLightEnv->ResolveAndApplyHDRSource(Desc);
		AppliedEnvironment = 0;
		AppliedProceduralSun = sun;
		return;
	}

	const int hdrIndex = ChooseEnvironment - 1;
	if (hdrIndex < 0 || hdrIndex >= (int)AllHDRFiles.size())
		return;

	SkyLightEnv->LoadTex(core::u8_ucs2(AllHDRFiles[hdrIndex]));
	SkyLightEnv->InvalidateCapturedEnvironment();
	SkyLightEnv->ResolveAndApplyHDRSource({});
	AppliedEnvironment = ChooseEnvironment;
}

void SkyLightEnvironmentDemoRunner::ShowTexture2D(RenderCore::RHICommandContext& Ctx, const std::shared_ptr<RenderCore::RHITexture2D>& Texture2D)
{
	if (!Texture2D || !ViewPort || !Window)
		return;

	std::shared_ptr<RenderCore::RHITexture2D> BackBuf = ViewPort->GetBackBuffer();
	if (!BackBuf)
		return;

	const float aspect = Texture2D->GetSize().w * 1.f / Texture2D->GetSize().h;
	int W = std::min(Window->GetWidth(), Texture2D->GetSize().w);
	int H = std::min(Window->GetHeight(), Texture2D->GetSize().h);
	W = std::min(W, (int)(H * aspect));
	H = std::min(H, (int)(W / aspect));

	RenderCore::FRHIRenderPassDesc Om = RenderCore::FRHIRenderPassDesc::SingleColorNoDepth(BackBuf);
	Om.DebugName = "SkyLightEnv_ShowTexture2D";
	{
		using A = RenderCore::FRDGResourceAccess;
		Om.DeclaredTextureBarriers.push_back(RenderCore::FRDGTextureBarrierDesc{ Texture2D, A::SRV, 0xFFFFFFFFu });
		Om.DeclaredTextureBarriers.push_back(RenderCore::FRDGTextureBarrierDesc{ BackBuf, A::RTV, 0xFFFFFFFFu });
	}
	RenderCore::FRHIRenderPassScope ShowScope(Ctx, std::move(Om));

	Ctx.SetViewPort((Window->GetWidth() - W) / 2, (Window->GetHeight() - H) / 2, W, H);

	RenderCore::GraphicsPipelineStateInitializer Init;
	Init.VertexShader = ShowTexture2DVS;
	Init.PixelShader = ShowTexture2DPS;
	Init.BlendState = RenderCore::RHICachedStates::BlendDisable;
	Init.DepthStencilState = RenderCore::RHICachedStates::DepthStateDisable;
	Init.RasterizerState = RenderCore::RHICachedStates::RasterizerStateCullNone;
	Ctx.RHISetGraphicsPipelineState(Init);

	Ctx.RHISetShaderSampler(RenderCore::SF_Pixel, 0, RenderCore::RHICachedStates::ClampPointSampler);
	Ctx.RHISetShaderTexture(RenderCore::SF_Pixel, 1, Texture2D);
	GET_UNIFORMDATA(PSRenderDemoContant).Exposure = Exposure;
	GET_UNIFORMDATA(PSRenderDemoContant).MipLevel = 0;
	RenderCore::RHI_UpdateAndBindUniformBuffer(Ctx, GET_SHADER_STRUCT_MEMBER(PSRenderDemoContant), RenderCore::SF_Pixel);
	Ctx.Draw(3);
}

void SkyLightEnvironmentDemoRunner::ShowCubeAsLongLat(RenderCore::RHICommandContext& Ctx,
	const std::shared_ptr<RenderCore::RHITextureCube>& Cube)
{
	if (!Cube || !ShowCubeEquirectPS || !ShowTexture2DVS || !ViewPort || !Window)
		return;

	std::shared_ptr<RenderCore::RHITexture2D> BackBuf = ViewPort->GetBackBuffer();
	if (!BackBuf)
		return;

	const int maxW = Window->GetWidth();
	const int maxH = Window->GetHeight();
	int H = std::min(maxH, maxW / 2);
	int W = std::min(maxW, H * 2);
	H = std::max(1, W / 2);

	RenderCore::FRHIRenderPassDesc Om = RenderCore::FRHIRenderPassDesc::SingleColorNoDepth(BackBuf);
	Om.DebugName = "SkyLightEnv_ShowCubeAsLongLat";
	{
		using A = RenderCore::FRDGResourceAccess;
		Om.DeclaredTextureBarriers.push_back(RenderCore::FRDGTextureBarrierDesc{ BackBuf, A::RTV, 0xFFFFFFFFu });
	}
	RenderCore::FRHIRenderPassScope ShowScope(Ctx, std::move(Om));

	Ctx.SetViewPort((Window->GetWidth() - W) / 2, (Window->GetHeight() - H) / 2, W, H);

	RenderCore::GraphicsPipelineStateInitializer Init;
	Init.VertexShader = ShowTexture2DVS;
	Init.PixelShader = ShowCubeEquirectPS;
	Init.BlendState = RenderCore::RHICachedStates::BlendDisable;
	Init.DepthStencilState = RenderCore::RHICachedStates::DepthStateDisable;
	Init.RasterizerState = RenderCore::RHICachedStates::RasterizerStateCullNone;
	Ctx.RHISetGraphicsPipelineState(Init);

	Ctx.RHISetShaderSampler(RenderCore::SF_Pixel, 0, RenderCore::RHICachedStates::ClampLinerSampler);
	Ctx.RHISetShaderTexture(RenderCore::SF_Pixel, 0, Cube);
	GET_UNIFORMDATA(PSRenderDemoContant).Exposure = Exposure;
	GET_UNIFORMDATA(PSRenderDemoContant).MipLevel = std::max(0, std::min(MipLevel, (int)Cube->GetNumMips() - 1));
	RenderCore::RHI_UpdateAndBindUniformBuffer(Ctx, GET_SHADER_STRUCT_MEMBER(PSRenderDemoContant), RenderCore::SF_Pixel);
	Ctx.Draw(3);
}

void SkyLightEnvironmentDemoRunner::ShowSHCubeMapDebugView(RenderCore::RHICommandContext& Ctx, const std::shared_ptr<RenderCore::RHITextureCube>& Cube)
{
	if (!CubeCross || !Cube || !ViewPort || !Window)
		return;

	std::shared_ptr<RenderCore::RHITexture2D> BackBuf = ViewPort->GetBackBuffer();
	if (!BackBuf)
		return;

	const uint32_t size = (uint32_t)std::min(Window->GetWidth(), Window->GetHeight());

	RenderCore::FRHIRenderPassDesc Om = RenderCore::FRHIRenderPassDesc::SingleColorNoDepth(BackBuf);
	Om.DebugName = "SkyLightEnv_CubeCrossDebug";
	{
		using A = RenderCore::FRDGResourceAccess;
		Om.DeclaredTextureBarriers.push_back(RenderCore::FRDGTextureBarrierDesc{ BackBuf, A::RTV, 0xFFFFFFFFu });
	}
	RenderCore::FRHIRenderPassScope ShowScope(Ctx, std::move(Om));

	Ctx.SetViewPort((Window->GetWidth() - (int)size) / 2, (Window->GetHeight() - (int)size) / 2, (int)size, (int)size);

	RenderCore::GraphicsPipelineStateInitializer Init;
	Init.VertexShader = CubeCrossVS;
	Init.PixelShader = CubeCrossPS;
	Init.BlendState = RenderCore::RHICachedStates::BlendDisable;
	Init.DepthStencilState = RenderCore::RHICachedStates::DepthStateDisable;
	Init.RasterizerState = RenderCore::RHICachedStates::RasterizerStateCullNone;
	Ctx.RHISetGraphicsPipelineState(Init);

	GET_UNIFORMDATA(CBPerObject).myPerObject_u_mCurrWorld = math::Matrix4x4();
	RenderCore::RHI_UpdateAndBindUniformBuffer(Ctx, GET_SHADER_STRUCT_MEMBER(CBPerObject), RenderCore::SF_Vertex);

	GET_UNIFORMDATA(CBPerFrame).myPerFrame.CameraCurrViewProj = math::Matrix4x4::MatrixOrthoLH(1.f, 1.f, -1.f, 1.f);
	RenderCore::RHI_UpdateAndBindUniformBuffer(Ctx, GET_SHADER_STRUCT_MEMBER(CBPerFrame), RenderCore::SF_Vertex);

	GET_UNIFORMDATA(PSRenderDemoContant).MipLevel = MipLevel;
	GET_UNIFORMDATA(PSRenderDemoContant).Exposure = Exposure;
	RenderCore::RHI_UpdateAndBindUniformBuffer(Ctx, GET_SHADER_STRUCT_MEMBER(PSRenderDemoContant), RenderCore::SF_Pixel);

	Ctx.RHISetShaderSampler(RenderCore::SF_Pixel, 0, RenderCore::RHICachedStates::ClampPointSampler);
	Ctx.RHISetShaderTexture(RenderCore::SF_Pixel, 0, Cube);

	CubeCross->Render(Ctx);
}
