#include "Engine/Engine.h"
#include "RHI/DynamicRHI.h"
#include "core/commandline.h"
#include "App/AppWindow.h"
#include "RHI/RHIViewPort.h"
#include "Thread/RenderThread.h"

namespace Engine
{
	MainEngine* GEngine = nullptr;

	struct MainEngineP
	{
		MainEngineP()
		{
			DynamicRHI = RenderCore::PlatformCreateDynamicRHI(RenderCore::RHIAPIType::E_D3D11);
		}
		std::shared_ptr<RenderCore::DynamicRHI> DynamicRHI;
		std::shared_ptr<RenderCore::RHIViewPort> MainViewPort;
		std::unique_ptr<RenderThread> RThread;
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
		if (Impl->DynamicRHI)
		{
			AppWin->idle.bind(std::bind(&MainEngine::Render, this), this);
			Impl->DynamicRHI->Init();
			Impl->MainViewPort = Impl->DynamicRHI->RHICreateViewport(AppWin->GetWnd(), AppWin->GetWidth(), AppWin->GetHeight(), false, RenderCore::PF_B8G8R8A8);
			Impl->RThread = std::make_unique<RenderThread>();
			Impl->RThread->Start();
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
		Impl->DynamicRHI = {};
		Impl->MainViewPort = {};
		Impl->RThread = {};
	}

	std::shared_ptr<RenderCore::DynamicRHI> MainEngine::GetRHI() const
	{
		return Impl->DynamicRHI;
	}

	void MainEngine::Render()
	{
		ENQUEUE_UNIQUE_RENDER_COMMAND(([Impl = Impl](){
			Impl->MainViewPort->Clear(1, 0, 0, 1);

			Impl->MainViewPort->Present();
		}));

	}

}