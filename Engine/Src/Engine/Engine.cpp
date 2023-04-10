#include "Engine/Engine.h"
#include "RHI/DynamicRHI.h"
#include "core/commandline.h"
#include "App/AppWindow.h"
#include "RHI/RHIViewPort.h"
#include "Thread/RenderThread.h"
#include "Scene/SceneView.h"
#include "Render/SceneRender.h"

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
		std::chrono::high_resolution_clock::time_point TStart;
		std::chrono::high_resolution_clock::time_point TEnd;
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
			AppWin->Idle.bind(std::bind(&MainEngine::Tick, this), this);
			AppWin->EvtSizeChanged.bind(std::bind(&MainEngine::OnSizeChanged, this,std::placeholders::_1), this);
			Impl->DynamicRHI->Init();
			std::shared_ptr<RenderCore::RHIViewPort> ViewPort = Impl->DynamicRHI->RHICreateViewport(AppWin->GetWnd(), AppWin->GetWidth(), AppWin->GetHeight(), false, RenderCore::PF_B8G8R8A8);
			Impl->RThread = std::make_unique<RenderThread>(Impl->DynamicRHI.get());
			Impl->RThread->Start();
			Impl->Scene->Init();
			Impl->SeRender->InitResource(ViewPort);
			Impl->TStart = std::chrono::high_resolution_clock::now();
		}
	}

	void MainEngine::ShutDown()
	{

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

	void MainEngine::Tick()
	{

		Impl->TEnd = std::chrono::high_resolution_clock::now();
		float DeltaTime = std::chrono::duration<float, std::milli>(Impl->TEnd - Impl->TStart).count();
		if (DeltaTime < (1000.0f / 60.f))
		{
			std::this_thread::sleep_for(0ms);
			return ;
		}

		Impl->Scene->Tick(DeltaTime);
		Impl->SeRender->Render();
		Impl->TStart = Impl->TEnd;
	}

	void MainEngine::OnSizeChanged(core::vec2i NewSize)
	{
		Impl->SeRender->Resize(NewSize.w, NewSize.h, false);
	}

}