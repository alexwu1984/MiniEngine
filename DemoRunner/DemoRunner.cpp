#include "win/win32.h"
#include "core/commandline.h"
#include "core/logger.h"
#include "core/system.h"

#include "App/AppWindow.h"
#include "RHI/DynamicRHI.h"
#include "RHI/RHIViewPort.h"

#include "Imgui/imgui.h"

#include <shellapi.h>
#include <combaseapi.h>

#include "core/strings.h"

#include "DemoRunner/Demos/IDemo.h"
#include "DemoRunner/Demos/PostProcessDemoRunner.h"
#include "DemoRunner/Demos/IBLRenderDemoRunner.h"
#include "DemoRunner/Demos/LiquidClassDemoRunner.h"
#include "RHI/DynamicRHI.h"

namespace
{
	float SecondsSince(ULONGLONG& lastTick)
	{
		const ULONGLONG now = ::GetTickCount64();
		const ULONGLONG dtMs = (lastTick == 0) ? 0 : (now - lastTick);
		lastTick = now;
		return (float)dtMs / 1000.0f;
	}
}

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE, PWSTR, int)
{
	int argc = 0;
	LPWSTR* argv = ::CommandLineToArgvW(::GetCommandLineW(), &argc);

	const std::wstring logPath = core::process_directory().wstring() + L"/Engine.log";
	core::global_logger::start(core::ucs2_u8(logPath), core::log_inf);

	::CoInitializeEx(nullptr, COINIT_MULTITHREADED);
	core::CommandLine::Get().SetCommandLine(argc, argv);

	int32_t width = 1920;
	int32_t height = 1080;
	core::CommandLine::Get().GetInteger("width", width);
	core::CommandLine::Get().GetInteger("height", height);

	// Force D3D12 for the minimal runner.
	auto RHI = RenderCore::PlatformCreateDynamicRHI(RenderCore::RHIAPIType::E_D3D12);
	if (!RHI)
	{
		core::logger::err() << "DemoRunner: failed to create DynamicRHI (D3D12)";
		::CoUninitialize();
		core::global_logger::stop();
		return 1;
	}

	auto window = std::make_shared<Engine::AppWindow>(hInstance);
	if (!window->CreateAppWindow(width, height))
	{
		core::logger::err() << "DemoRunner: failed to create window";
		::CoUninitialize();
		core::global_logger::stop();
		return 1;
	}

	RHI->Init();

	std::shared_ptr<RenderCore::RHIViewPort> viewPort =
		RHI->RHICreateViewport(window->GetWnd(), window->GetWidth(), window->GetHeight(), false, RenderCore::PF_B8G8R8A8);

	if (!viewPort)
	{
		core::logger::err() << "DemoRunner: failed to create viewport";
		RHI->Shutdown();
		::CoUninitialize();
		core::global_logger::stop();
		return 1;
	}

	// Minimal fullscreen demo.
	std::string demoId = "post";
	core::CommandLine::Get().GetString("demo", demoId);

	std::unique_ptr<DemoRunner::IDemo> demo;
	if (demoId == "ibl")
		demo = std::make_unique<DemoRunner::IBLRenderDemoRunner>(RHI.get());
	else if (demoId == "liquid")
		demo = std::make_unique<DemoRunner::LiquidClassDemoRunner>(RHI.get());
	else
		demo = std::make_unique<DemoRunner::PostProcessDemoRunner>();

	demo->Init(RHI.get(), viewPort, window);

	// Handle resize directly; this runner avoids SceneRender/FrameGraph completely.
	window->EvtSizeChanged.bind([viewPort](core::vec2i sz) {
		if (viewPort && sz.w > 0 && sz.h > 0)
			viewPort->Resize((uint32_t)sz.w, (uint32_t)sz.h, false);
	}, window.get());

	ULONGLONG lastTick = 0;
	window->Idle.bind([&]() {
		const float dt = SecondsSince(lastTick);

		if (!viewPort)
			return;

		// Begin ImGui frame (done inside ViewPort->Prepare()).
		const auto cc = demo->GetClearColor();
		viewPort->Clear(core::FLinearColor(cc.r, cc.g, cc.b, cc.a));
		viewPort->Prepare();

		// Minimal UI: show command line knobs and frame dt.
		if (!core::CommandLine::Get().GetName("noimgui"))
		{
			ImGui::SetNextWindowPos(ImVec2(10, 10), ImGuiCond_Once);
			ImGui::SetNextWindowSize(ImVec2(360, 120), ImGuiCond_Once);
			if (ImGui::Begin("DemoRunner", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
			{
				ImGui::Text("demo=%s", demo->GetName());
				ImGui::Text("dt: %.3f ms", dt * 1000.0f);
				{
					int memmonV = 0;
					core::CommandLine::Get().GetInteger("d3d12_memmon", memmonV);
					int stacksV = 0;
					core::CommandLine::Get().GetInteger("d3d12_memmon_stacks", stacksV);
					int gpudevV = 1;
					core::CommandLine::Get().GetInteger("d3d12_gpudev", gpudevV);
					const bool memmonOn = RenderCore::D3D12RHI_ShouldEnableMemMon();
					const bool stacksOn = RenderCore::D3D12RHI_ShouldEnableMemMonStacks();
					ImGui::Text("d3d12_memmon=%d (effective=%s)", memmonV, memmonOn ? "on" : "off");
					ImGui::Text("d3d12_memmon_stacks=%d (effective=%s)", stacksV, stacksOn ? "on" : "off");
					ImGui::Text("d3d12_gpudev=%d", gpudevV);
					ImGui::Text("d3ddebug=%s / dxdebug=%s",
						core::CommandLine::Get().GetName("d3ddebug") ? "on" : "off",
						core::CommandLine::Get().GetName("dxdebug") ? "on" : "off");
				}
				ImGui::Separator();
				demo->OnGui();
			}
			ImGui::End();
		}

		// Draw demo.
		auto ctx = RHI->GetDefaultCommandContext();
		if (!ctx)
			return;

		const int32_t w = window->GetWidth();
		const int32_t h = window->GetHeight();
		ctx->SetViewPort(0, 0, w, h);
		demo->Draw(*ctx, viewPort, dt);

		// Render ImGui + present (ImGui render happens inside Present()).
		viewPort->Present();
	}, window.get());

	const int exitCode = window->RunLoop();

	// Make sure window events drop callbacks before leak report.
	window->Idle.unbindall();
	window->EvtSizeChanged.unbindall();

	// Release demo before tearing down GPU-backed viewport resources.
	demo.reset();

	// GPU must finish in-flight work on swapchain/backbuffers before we destroy the viewport.
	// Doing viewPort.reset() first can leave the queue busy while RTVs are freed; with the D3D12
	// debug layer this often surfaces as a hang or long stall at shutdown.
	RHI->Wait();
	viewPort.reset();

	RHI->Shutdown();
	RHI.reset();
	RenderCore::ReleasePlatformModule();

	::CoUninitialize();
	core::global_logger::stop();
	return exitCode;
}

