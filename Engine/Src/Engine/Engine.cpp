#include "Engine/Engine.h"
#include "RHI/DynamicRHI.h"
#include "core/commandline.h"
#include "App/AppWindow.h"
#include "RHI/RHIViewPort.h"

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
			Data->DynamicRHI->Init();
			Data->MainViewPort = Data->DynamicRHI->RHICreateViewport(AppWin->GetWnd(), 10, 10, false, RenderCore::PF_B8G8R8A8);
		}
	}

	std::shared_ptr<RenderCore::DynamicRHI> MainEngine::GetRHI() const
	{
		return Data->DynamicRHI;
	}

}