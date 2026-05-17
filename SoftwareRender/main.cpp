#include "win/win32.h"
#include "core/commandline.h"
#include "core/logger.h"
#include "core/system.h"
#include "App/AppWindow.h"
#include "RHI/DynamicRHI.h"
#include "RHI/RHICommandContext.h"
#include "RHI/RHIViewPort.h"
#include "D3D12/D3D12RHIRecording.h"
#include "Imgui/imgui.h"
#include <shellapi.h>
#include "core/strings.h"
#include "ViewerApp.h"

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
	core::scoped_com_mta_init com_mta;

	const std::wstring logPath = core::process_directory().wstring() + L"/SoftwareRender.log";
	core::global_logger::start(core::ucs2_u8(logPath), core::log_inf);

	core::CommandLine::Get().SetCommandLine(argc, argv);

	int32_t width = 1280;
	int32_t height = 960;
	core::CommandLine::Get().GetInteger("width", width);
	core::CommandLine::Get().GetInteger("height", height);

	auto RHI = RenderCore::PlatformCreateDynamicRHI(RenderCore::RHIAPIType::E_D3D12);
	if (!RHI)
	{
		core::logger::err() << "SoftwareRender: failed to create DynamicRHI (D3D12)";
		core::global_logger::stop();
		return 1;
	}

	auto window = std::make_shared<Engine::AppWindow>(hInstance);
	if (!window->CreateAppWindow(width, height))
	{
		core::logger::err() << "SoftwareRender: failed to create window";
		core::global_logger::stop();
		return 1;
	}

	RHI->Init();

	std::shared_ptr<RenderCore::RHIViewPort> viewPort =
		RHI->RHICreateViewport(window->GetWnd(), window->GetWidth(), window->GetHeight(), false, RenderCore::PF_B8G8R8A8);

	if (!viewPort)
	{
		core::logger::err() << "SoftwareRender: failed to create viewport";
		RHI->Shutdown();
		core::global_logger::stop();
		return 1;
	}

	ViewerApp app;
	app.BuildCpuRayTraceScene();
	{
		RHI->RHIBeginFrame();
		{
			const RenderCore::D3D12RHI_ScopedRecordingContext ScopedInsideRecordingFrame(
				RenderCore::ERHIRecordingContextScope::InsideFrameTick);
			app.GpuInit(RHI.get());
		}
		RHI->RHIEndFrame();
	}

	window->EvtSizeChanged.bind(
		[viewPort](core::vec2i sz) {
			if (viewPort && sz.w > 0 && sz.h > 0)
				viewPort->Resize((uint32_t)sz.w, (uint32_t)sz.h, false);
		},
		window.get());

	ULONGLONG lastTick = 0;
	window->Idle.bind(
		[&]() {
			const float dt = SecondsSince(lastTick);

			if (!viewPort)
				return;

			RHI->RHIBeginFrame();
			{
				const RenderCore::D3D12RHI_ScopedRecordingContext ScopedInsideRecordingFrame(
					RenderCore::ERHIRecordingContextScope::InsideFrameTick);

				const auto cc = app.GetClearColor();
				viewPort->Clear(core::FLinearColor(cc.R, cc.G, cc.B, cc.A));
				viewPort->Prepare();

				ImGui::SetNextWindowPos(ImVec2(10, 10), ImGuiCond_Once);
				ImGui::SetNextWindowSize(ImVec2(320, 80), ImGuiCond_Once);
				if (ImGui::Begin("SoftwareRender", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
				{
					ImGui::Text("dt: %.3f ms", dt * 1000.0f);
				}
				ImGui::End();

				if (auto ctx = RHI->GetDefaultCommandContext())
				{
					const int32_t w = window->GetWidth();
					const int32_t h = window->GetHeight();
					ctx->SetViewPort(0, 0, w, h);
					app.GpuDraw(*ctx, viewPort, window, dt);

					viewPort->Present();
				}
			}

			RHI->RHIEndFrame();
		},
		window.get());

	const int exitCode = window->RunLoop();

	window->Idle.unbindall();
	window->EvtSizeChanged.unbindall();

	RHI->Wait();
	viewPort.reset();

	RHI->Shutdown();
	RHI.reset();
	RenderCore::ReleasePlatformModule();

	core::global_logger::stop();
	return exitCode;
}
