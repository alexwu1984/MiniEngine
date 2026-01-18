#include "Engine/Engine.h"
#include "core/commandline.h"
#include "App/AppWindow.h"
#include "RHI/RHIViewPort.h"
#include "Thread/RenderThread.h"
#include "Scene/SceneView.h"
#include "Render/SceneRender.h"
#include "win/high_precision_tick.h"

namespace Engine
{
	MainEngine* GEngine = nullptr;

	struct MainEnginePrivate
	{
		MainEnginePrivate()
		{
			Scene = std::make_shared<SceneView>();
			SeRender = std::make_shared<SceneRender>(Scene);
		}
		std::shared_ptr< AppWindow> AppWin;
		std::shared_ptr<RenderCore::DynamicRHI> DynamicRHI;
		std::unique_ptr<RenderThread> RThread;
		std::shared_ptr<SceneView> Scene;
		std::shared_ptr<SceneRender> SeRender;
		win32::HighPrecisionTick GameTick;
		std::wstring ModelPath;
		std::atomic_bool NeedResize = false;
		core::vec2i NewSize;

	};

	MainEngine::MainEngine()
		:d_ptr(new MainEnginePrivate())
	{
		GEngine = this;
	}

	MainEngine::~MainEngine()
	{
		delete d_ptr;
		RenderCore::ReleasePlatformModule();
	}

	void MainEngine::Init(std::shared_ptr<AppWindow> AppWin, RenderCore::RHIAPIType ApiType)
	{
		C_P(MainEngine);
		d->DynamicRHI = RenderCore::PlatformCreateDynamicRHI(ApiType);
		d->AppWin = AppWin;
		if (d->DynamicRHI)
		{
			d->GameTick.SigTick.bind(std::bind(&MainEngine::Tick, this,std::placeholders::_1), this);
			AppWin->EvtSizeChanged.bind(std::bind(&MainEngine::OnSizeChanged, this,std::placeholders::_1), this);
			d->DynamicRHI->Init();
			std::shared_ptr<RenderCore::RHIViewPort> ViewPort = d->DynamicRHI->RHICreateViewport(AppWin->GetWnd(), AppWin->GetWidth(), AppWin->GetHeight(), false, RenderCore::PF_B8G8R8A8);
			d->RThread = std::make_unique<RenderThread>(d->DynamicRHI.get());
			d->Scene->Init();
			d->SeRender->InitResource(ViewPort);
		}
	}

	void MainEngine::StartThread()
	{
		C_P(MainEngine);
		d->RThread->Start();
		d->GameTick.Start("GameThread", 120, win32::HighPrecisionTick::ThreadPriority::Highest);
	}

	void MainEngine::ShutDown()
	{
		C_P(MainEngine);
		d->GameTick.Stop();
		if (d->RThread)
		{
			d->RThread->Stop();
		}
		if (d->DynamicRHI)
		{
			d->DynamicRHI->Wait();
		}

		d->SeRender = {};
		d->Scene = {};
		d->RThread = {};
		if (d->DynamicRHI)
		{
			d->DynamicRHI->Shutdown();
		}
		d->DynamicRHI = {};
	}

	void MainEngine::LoadConfig(const std::wstring& FileName)
	{
		C_P(MainEngine);
		if (d->SeRender)
		{
			d->SeRender->LoadConfig(FileName);
		}
		d->ModelPath = std::filesystem::path(FileName).parent_path();
	}

	std::wstring MainEngine::GetModelPath() const
	{
		C_P(const MainEngine);
		return d->ModelPath;
	}

	std::shared_ptr<RenderCore::DynamicRHI> MainEngine::GetRHI() const
	{
		C_P(const MainEngine);
		return d->DynamicRHI;
	}

	std::shared_ptr<AppWindow> MainEngine::GetAppWindow() const
	{
		C_P(const MainEngine);
		return d->AppWin;
	}

	std::shared_ptr<SceneView> MainEngine::GetScene() const
	{
		C_P(const MainEngine);
		return d->Scene;
	}

	std::shared_ptr<SceneRender> MainEngine::GetSceneRender() const
	{
		C_P(const MainEngine);
		return d->SeRender;
	}

	void MainEngine::Tick(float DeltaTime)
	{
		C_P(MainEngine);
		d->Scene->Tick(DeltaTime);
		d->SeRender->Render(DeltaTime);
		if (d->NeedResize)
		{
			d->SeRender->Resize(d->NewSize.w, d->NewSize.h, false);
			d->NewSize = {};
			d->NeedResize = false;
		}
	}

	void MainEngine::OnSizeChanged(core::vec2i NewSize)
	{
		C_P(MainEngine);
		d->NeedResize = true;
		d->NewSize = NewSize;
	}

}