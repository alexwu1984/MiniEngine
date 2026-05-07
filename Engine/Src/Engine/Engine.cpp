#include "Engine/Engine.h"
#include "core/commandline.h"
#include "App/AppWindow.h"
#include "RHI/RHIViewPort.h"
#include "Thread/RenderThread.h"
#include "Engine/Thread/RHISubmissionThread.h"
#include "RHI/RHIThreadPolicy.h"
#include "Scene/SceneManager.h"
#include "Scene/World.h"
#include "Scene/GameViewportClient.h"
#include "Render/WorldSceneRender.h"
#include "win/high_precision_tick.h"
#include "Engine/Render/RenderTexturePool.h"
#include "RHI/DynamicRHI.h"
#include "Engine/ComErrorLog.h"
#include <functional>

namespace Engine
{
	MainEngine* GEngine = nullptr;

	struct MainEnginePrivate
	{
		explicit MainEnginePrivate(MainEngine* Owner)
		{
			SceneMgr = std::make_shared<SceneManager>();
			ViewportClient = std::make_shared<GameViewportClient>(std::weak_ptr<World>(SceneMgr->GetWorld()));
			SeRender = std::make_shared<FWorldSceneRender>(std::weak_ptr<World>(SceneMgr->GetWorld()));
			SceneMgr->AttachClients(Owner, ViewportClient, SeRender);
		}
		std::shared_ptr<AppWindow> AppWin;
		std::shared_ptr<RenderCore::DynamicRHI> DynamicRHI;
		std::unique_ptr<RenderThread> RThread;
		std::unique_ptr<RHISubmissionThread> RHISubmitThread;
		RenderCore::RHIAPIType InitApiType = RenderCore::RHIAPIType::E_D3D12;
		std::shared_ptr<SceneManager> SceneMgr;
		std::shared_ptr<GameViewportClient> ViewportClient;
		std::shared_ptr<FWorldSceneRender> SeRender;
		win32::HighPrecisionTick GameTick;
		std::wstring ModelPath;
		std::atomic_bool NeedResize = false;
		core::vec2i NewSize;
		std::function<void()> EndFrameTickCallback;

		bool bFlushRenderQueueEndOfTick = false;
		bool bGpuIdleWaitEndOfTick = false;
	};

	MainEngine::MainEngine()
		: d_ptr(new MainEnginePrivate(this))
	{
		GEngine = this;
	}

	MainEngine::~MainEngine()
	{
		delete d_ptr;
	}

	void MainEngine::Init(std::shared_ptr<AppWindow> AppWin, RenderCore::RHIAPIType ApiType)
	{
		C_P(MainEngine);
		d->InitApiType = ApiType;
		d->DynamicRHI = RenderCore::PlatformCreateDynamicRHI(ApiType);
		d->AppWin = AppWin;
		if (d->DynamicRHI)
		{
			d->DynamicRHI->SetFrameCallbacks(
				[]() { Engine::RenderTexturePool::Get().BeginFrame(); },
				[]() { Engine::RenderTexturePool::Get().EndFrame(); });

			d->GameTick.SigTick.bind(std::bind(&MainEngine::Tick, this, std::placeholders::_1), this);
			AppWin->EvtSizeChanged.bind(std::bind(&MainEngine::OnSizeChanged, this, std::placeholders::_1), this);
			d->DynamicRHI->Init();
			std::shared_ptr<RenderCore::RHIViewPort> ViewPort = d->DynamicRHI->RHICreateViewport(AppWin->GetWnd(), AppWin->GetWidth(), AppWin->GetHeight(), false, RenderCore::PF_B8G8R8A8);
			d->RThread = std::make_unique<RenderThread>(d->DynamicRHI.get());
			d->ViewportClient->Init(AppWin);
			d->SeRender->InitResource(ViewPort);
		}
	}

	void MainEngine::StartRenderWorkerThreads()
	{
		C_P(MainEngine);
		const bool bWantRHIWorker =
			(d->InitApiType == RenderCore::RHIAPIType::E_D3D12) && !core::CommandLine::Get().GetSwitch("norhithread");
		if (bWantRHIWorker)
		{
			d->RHISubmitThread = std::make_unique<RHISubmissionThread>();
			d->RHISubmitThread->Start();
			RHISubmissionThread* SubmitWorker = d->RHISubmitThread.get();
			RenderCore::RHI_SetSubmissionExecutor([SubmitWorker](std::function<void()> RHIWork) {
				SubmitWorker->EnqueueAndWait(std::move(RHIWork));
			});
		}
		d->RThread->Start();

		// rendersync gpuwait maxrenderframes: RenderQueueSynchronization.h; default cap tracks RHIRecommendedParallelFrameResourceSlots
		if (d->SeRender)
		{
			uint32_t maxInflight =
				d->DynamicRHI ? d->DynamicRHI->RHIRecommendedParallelFrameResourceSlots() : 2u;
			int mfParsed = 0;
			if (core::CommandLine::Get().GetInteger("maxrenderframes", mfParsed))
				maxInflight = mfParsed <= 0 ? 0u : static_cast<uint32_t>(mfParsed);
			d->SeRender->SetMaxSceneFramesInFlight(maxInflight);
			d->bFlushRenderQueueEndOfTick = core::CommandLine::Get().GetSwitch("rendersync");
			d->bGpuIdleWaitEndOfTick = core::CommandLine::Get().GetSwitch("gpuwait");
		}
	}

	void MainEngine::StartGameLoopTick()
	{
		C_P(MainEngine);
		d->GameTick.Start("GameThread", 120, win32::HighPrecisionTick::ThreadPriority::Highest);
	}

	void MainEngine::ShutDown()
	{
		C_P(MainEngine);
		d->EndFrameTickCallback = {};
		d->GameTick.SigTick.unbind(this);
		if (d->AppWin)
			d->AppWin->EvtSizeChanged.unbind(this);
		d->GameTick.Stop();
		if (d->RThread)
		{
			d->RThread->Stop();
		}
		RenderCore::RHI_ClearSubmissionExecutor();
		if (d->RHISubmitThread)
		{
			d->RHISubmitThread->Stop();
			d->RHISubmitThread.reset();
		}
		if (d->DynamicRHI)
		{
			d->DynamicRHI->Wait();
		}

		d->SeRender = {};
		d->ViewportClient = {};
		d->SceneMgr.reset();
		Engine::RenderTexturePool::Get().Clear();
		d->RThread = {};
		if (d->DynamicRHI)
		{
			d->DynamicRHI->Shutdown();
		}
		d->DynamicRHI = {};
		RenderCore::ReleasePlatformModule();
	}

	void MainEngine::LoadConfig(const std::wstring& FileName, const nlohmann::json& Root)
	{
		C_P(MainEngine);
		if (d->SeRender)
		{
			d->SeRender->LoadConfig(Root);
		}
		d->ModelPath = std::filesystem::path(FileName).parent_path();
	}

	void MainEngine::RebindSceneRenderToCurrentWorld()
	{
		C_P(MainEngine);
		if (!d->SeRender || !d->SceneMgr || !d->ViewportClient)
			return;
		d->SeRender->SetWorld(std::weak_ptr<World>(d->SceneMgr->GetWorld()));
		d->SceneMgr->AttachClients(this, d->ViewportClient, d->SeRender);
	}

	void MainEngine::FinalizeViewportRenderingAfterSceneCut()
	{
		C_P(MainEngine);
		if (d->SceneMgr)
		{
			if (const auto w = d->SceneMgr->GetWorld())
				w->InvalidatePrimaryViewStateAfterSceneCut();
		}
		if (d->SeRender)
			d->SeRender->NotifyWorldRenderingSceneChanged();
	}

	void MainEngine::ReloadSceneJson(const std::wstring& JsonPath)
	{
		C_P(MainEngine);
		if (!d->SceneMgr)
			return;
		d->SceneMgr->ReloadSceneJson(JsonPath);
	}

	std::shared_ptr<SceneManager> MainEngine::GetSceneManager() const
	{
		C_P(const MainEngine);
		return d->SceneMgr;
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

	std::shared_ptr<World> MainEngine::GetWorld() const
	{
		C_P(const MainEngine);
		return d->SceneMgr ? d->SceneMgr->GetWorld() : nullptr;
	}

	std::shared_ptr<GameViewportClient> MainEngine::GetViewportClient() const
	{
		C_P(const MainEngine);
		return d->ViewportClient;
	}

	std::shared_ptr<FWorldSceneRender> MainEngine::GetSceneRender() const
	{
		C_P(const MainEngine);
		return d->SeRender;
	}

	RenderCore::RHIAPIType MainEngine::GetInitRHIApiType() const noexcept
	{
		C_P(const MainEngine);
		return d->InitApiType;
	}

	void MainEngine::SetEndFrameTickCallback(std::function<void()> Callback)
	{
		C_P(MainEngine);
		d->EndFrameTickCallback = std::move(Callback);
	}

	void MainEngine::Tick(float DeltaTime)
	{
		try
		{
			C_P(MainEngine);
			if (d->ViewportClient)
				d->ViewportClient->Tick(DeltaTime);
			d->SeRender->Render(DeltaTime);
			if (d->NeedResize)
			{
				d->SeRender->Resize(d->NewSize.w, d->NewSize.h, false);
				d->NewSize = {};
				d->NeedResize = false;
			}
			if (d->EndFrameTickCallback)
				d->EndFrameTickCallback();
			if (d->SeRender && (d->bFlushRenderQueueEndOfTick || d->bGpuIdleWaitEndOfTick))
				d->SeRender->EndGameThreadFrameSync(d->bFlushRenderQueueEndOfTick || d->bGpuIdleWaitEndOfTick, d->bGpuIdleWaitEndOfTick);
		}
		catch (const _com_error& e)
		{
			LogComErrorToEngineLog(L"MainEngine::Tick(game_thread)", e);
		}
		catch (const std::exception& e)
		{
			LogStdExceptionToEngineLog(L"MainEngine::Tick(game_thread)", e);
		}
		catch (...)
		{
			LogUnknownExceptionToEngineLog(L"MainEngine::Tick(game_thread)");
		}
	}

	void MainEngine::OnSizeChanged(core::vec2i NewSize)
	{
		C_P(MainEngine);
		d->NeedResize = true;
		d->NewSize = NewSize;
	}

} // namespace Engine
