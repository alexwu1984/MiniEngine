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
		:Data(std::make_shared<MainEngineP>())
	{
		GEngine = this;
	}

	MainEngine::~MainEngine()
	{

	}

	void MainEngine::Init(std::shared_ptr< AppWindow> AppWin)
	{
		if (Data->DynamicRHI)
		{
			AppWin->idle.bind(std::bind(&MainEngine::Render, this), this);
			Data->DynamicRHI->Init();
			Data->MainViewPort = Data->DynamicRHI->RHICreateViewport(AppWin->GetWnd(), AppWin->GetWidth(), AppWin->GetHeight(), false, RenderCore::PF_B8G8R8A8);
			Data->RThread = std::make_unique<RenderThread>();
			Data->RThread->Start();
		}
	}

	void MainEngine::ShutDown()
	{
		if (Data->RThread)
		{
			Data->RThread->Stop();
		}
		if (Data->DynamicRHI)
		{
			Data->DynamicRHI->Shutdown();
		}
		Data->DynamicRHI = {};
		Data->MainViewPort = {};
		Data->RThread = {};
	}

	std::shared_ptr<RenderCore::DynamicRHI> MainEngine::GetRHI() const
	{
		return Data->DynamicRHI;
	}

	void MainEngine::Render()
	{
		ENQUEUE_UNIQUE_RENDER_COMMAND([Data = Data](){
			Data->MainViewPort->Clear(1, 0, 0, 1);

			Data->MainViewPort->Present();
			});

	}

}