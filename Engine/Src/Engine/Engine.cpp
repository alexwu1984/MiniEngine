#include "Engine/Engine.h"
#include "RHI/DynamicRHI.h"
#include "core/commandline.h"
#include "App/AppWindow.h"
#include "RHI/RHIViewPort.h"
#include "Thread/RenderThread.h"
#include "Scene/SceneView.h"
#include "Render/SceneRender.h"
#include "win/high_precision_tick.h"
#include "Render/Imgui/imgui.h"
#include "Render/Imgui/imgui_impl_win32.h"

namespace Engine
{
	MainEngine* GEngine = nullptr;

	struct MainEngineP
	{
		MainEngineP()
		{
			DynamicRHI = RenderCore::PlatformCreateDynamicRHI(RenderCore::RHIAPIType::E_D3D11);
			Scene = std::make_shared<SceneView>();
			SeRender = std::make_shared<SceneRender>(Scene);
		}
		std::shared_ptr< AppWindow> AppWin;
		std::shared_ptr<RenderCore::DynamicRHI> DynamicRHI;
		std::unique_ptr<RenderThread> RThread;
		std::shared_ptr<SceneView> Scene;
		std::shared_ptr<SceneRender> SeRender;
		win32::HighPrecisionTick GameTick;
	};

	MainEngine::MainEngine()
		:Impl(std::make_shared<MainEngineP>())
	{
		GEngine = this;
	}

	MainEngine::~MainEngine()
	{

	}

	void MainEngine::Init(std::shared_ptr< AppWindow> AppWin)
	{
		Impl->AppWin = AppWin;
		if (Impl->DynamicRHI)
		{
			// Setup Dear ImGui context
			IMGUI_CHECKVERSION();
			ImGui::CreateContext();
			ImGuiIO& io = ImGui::GetIO(); (void)io;
			io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
			io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls

			Impl->GameTick.SigTick.bind(std::bind(&MainEngine::Tick, this,std::placeholders::_1), this);
			AppWin->EvtSizeChanged.bind(std::bind(&MainEngine::OnSizeChanged, this,std::placeholders::_1), this);
			Impl->DynamicRHI->Init();
			std::shared_ptr<RenderCore::RHIViewPort> ViewPort = Impl->DynamicRHI->RHICreateViewport(AppWin->GetWnd(), AppWin->GetWidth(), AppWin->GetHeight(), false, RenderCore::PF_B8G8R8A8);
			Impl->RThread = std::make_unique<RenderThread>(Impl->DynamicRHI.get());
			Impl->RThread->Start();
			Impl->GameTick.Start("GameThread", 60, win32::HighPrecisionTick::ThreadPriority::Highest);
			Impl->Scene->Init();
			Impl->SeRender->InitResource(ViewPort);
		}
	}

	void MainEngine::ShutDown()
	{
		Impl->GameTick.Stop();
		if (Impl->RThread)
		{
			Impl->RThread->Stop();
		}
		if (Impl->DynamicRHI)
		{
			Impl->DynamicRHI->Shutdown();
		}
		Impl->SeRender = {};
		Impl->Scene = {};
		Impl->DynamicRHI = {};
		Impl->RThread = {};
		
	}

	void MainEngine::LoadConfig(const std::wstring& FileName)
	{
		if (Impl->SeRender)
		{
			Impl->SeRender->LoadConfig(FileName);
		}
	}

	std::shared_ptr<RenderCore::DynamicRHI> MainEngine::GetRHI() const
	{
		return Impl->DynamicRHI;
	}

	std::shared_ptr<AppWindow> MainEngine::GetAppWindow() const
	{
		return Impl->AppWin;
	}

	std::shared_ptr<SceneView> MainEngine::GetScene() const
	{
		return Impl->Scene;
	}

	std::shared_ptr<Engine::SceneRender> MainEngine::GetSceneRender() const
	{
		return Impl->SeRender;
	}

	void MainEngine::Tick(float DeltaTime)
	{
		Impl->Scene->Tick(DeltaTime);
		Impl->SeRender->Render();
	}

	void MainEngine::OnSizeChanged(core::vec2i NewSize)
	{
		Impl->SeRender->Resize(NewSize.w, NewSize.h, false);
	}

}