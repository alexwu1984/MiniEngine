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
#include "RHI/RHIStructuredBuffer.h"
#include "Engine/ComErrorLog.h"
#include "core/logger.h"
#include "core/wall_timer.h"
#include <array>
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
		core::WallSplitTimer Wall;
		d->InitApiType = ApiType;
		d->DynamicRHI = RenderCore::PlatformCreateDynamicRHI(ApiType);
		const double MsPlatformCreateRHI = Wall.split_ms();
		d->AppWin = AppWin;
		if (d->DynamicRHI)
		{
			d->DynamicRHI->SetFrameCallbacks(
				[]() { Engine::RenderTexturePool::Get().BeginFrame(); },
				[]() { Engine::RenderTexturePool::Get().EndFrame(); });

			d->GameTick.SigTick.bind(std::bind(&MainEngine::Tick, this, std::placeholders::_1), this);
			AppWin->EvtSizeChanged.bind(std::bind(&MainEngine::OnSizeChanged, this, std::placeholders::_1), this);
			d->DynamicRHI->Init();
			const double MsDynamicRHIInit = Wall.split_ms();
			std::shared_ptr<RenderCore::RHIViewPort> ViewPort = d->DynamicRHI->RHICreateViewport(AppWin->GetWnd(), AppWin->GetWidth(), AppWin->GetHeight(), false, RenderCore::PF_B8G8R8A8);
			const double MsCreateViewport = Wall.split_ms();

			// Smoke test for RHIStructuredBuffer plumbing (clustered Forward+ prep): create + update a static and a
			// dynamic 16-element buffer, then drop them. Catches misconfigured backends / D3D validation errors at
			// engine init rather than at the first clustered-light pass. Stripped to a log line on success.
			{
				const std::array<uint32_t, 16> InitialPayload{ 0u, 1u, 2u, 3u, 4u, 5u, 6u, 7u, 8u, 9u, 10u, 11u, 12u, 13u, 14u, 15u };
				std::shared_ptr<RenderCore::RHIStructuredBuffer> StaticBuf = d->DynamicRHI->RHICreateStructuredBuffer(sizeof(uint32_t), (uint32_t)InitialPayload.size(),
																													  RenderCore::BUF_Static, InitialPayload.data());
				std::shared_ptr<RenderCore::RHIStructuredBuffer> DynamicBuf = d->DynamicRHI->RHICreateStructuredBuffer(sizeof(uint32_t), (uint32_t)InitialPayload.size(),
																													   RenderCore::BUF_Dynamic, nullptr);
				if (DynamicBuf)
					DynamicBuf->UpdateStructuredBuffer(InitialPayload.data(), (uint32_t)(InitialPayload.size() * sizeof(uint32_t)));
				if (core::perf::ShouldEmitPerfInfLogs())
				{
					core::inf() << core::perf::hdr(core::perf::kEngine, "RHIStructuredBufferSmoke") << "static=" << (StaticBuf ? 1 : 0)
								<< " dynamic=" << (DynamicBuf ? 1 : 0) << " stride=" << sizeof(uint32_t) << " count=" << InitialPayload.size() << "\n";
				}
			}

			d->RThread = std::make_unique<RenderThread>(d->DynamicRHI.get());
			const double MsRenderThreadCtor = Wall.split_ms();
			d->ViewportClient->Init(AppWin);
			const double MsViewportClientInit = Wall.split_ms();
			d->SeRender->InitResource(ViewPort);
			const double MsSceneRenderInitResourceEnqueue = Wall.split_ms();
			const double MsTotal = Wall.total_ms();
			if (core::perf::ShouldEmitPerfInfLogs())
			{
				core::inf() << core::perf::hdr(core::perf::kEngine, "Init") << "total_ms=" << MsTotal << " platform_create_rhi_ms=" << MsPlatformCreateRHI
							<< " dynamic_rhi_init_ms=" << MsDynamicRHIInit << " create_viewport_ms=" << MsCreateViewport
							<< " render_thread_ctor_ms=" << MsRenderThreadCtor << " viewport_client_init_ms=" << MsViewportClientInit
							<< " scene_render_init_resource_enqueue_ms=" << MsSceneRenderInitResourceEnqueue
							<< " note=async_render_rt_init_see_Perf|render_rt|WorldSceneRenderInit\n";
			}
		}
	}

	void MainEngine::StartRenderWorkerThreads()
	{
		C_P(MainEngine);
		core::WallSplitTimer Wall;
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
		const double MsWorkers = Wall.total_ms();
		if (core::perf::ShouldEmitPerfInfLogs())
		{
			core::inf() << core::perf::hdr(core::perf::kEngine, "StartRenderWorkers") << "wall_ms=" << MsWorkers << " rhi_submit_worker=" << (bWantRHIWorker ? 1 : 0)
						<< "\n";
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
			core::WallSplitTimer TickWall;

			if (d->ViewportClient)
				d->ViewportClient->Tick(DeltaTime);
			const double msViewport = TickWall.split_ms();

			// Apply pending swapchain/scene-target resize before submitting this frame so the first frame after WM_SIZE matches client pixels.
			if (d->NeedResize)
			{
				d->SeRender->Resize(d->NewSize.w, d->NewSize.h, false);
				d->NewSize = {};
				d->NeedResize = false;
			}
			const double msResize = TickWall.split_ms();

			d->SeRender->Render(DeltaTime);
			const double msSubmitScene = TickWall.split_ms();

			if (d->EndFrameTickCallback)
				d->EndFrameTickCallback();
			const double msEndFrameCb = TickWall.split_ms();

			if (d->SeRender && (d->bFlushRenderQueueEndOfTick || d->bGpuIdleWaitEndOfTick))
				d->SeRender->EndGameThreadFrameSync(d->bFlushRenderQueueEndOfTick || d->bGpuIdleWaitEndOfTick, d->bGpuIdleWaitEndOfTick);
			const double msGpuSync = TickWall.split_ms();

			const double msTickTotal = TickWall.total_ms();

			// HighPrecisionTick DeltaTime is wall time between tick entry points; a large value usually means the prior Tick blocked (flush/sync/heavy work).
			const bool bLongGap = DeltaTime >= 0.5f;
			const bool bHeavyWork = msTickTotal >= 80.0 || msSubmitScene >= 80.0 || msGpuSync >= 80.0 || msViewport >= 80.0;
			if ((bLongGap || bHeavyWork) && core::perf::ShouldEmitPerfInfLogs())
			{
				core::inf() << core::perf::hdr(core::perf::kTick, "Heavy") << "high_precision_delta_time_s=" << DeltaTime
							<< " viewport_client_ms=" << msViewport << " submit_scene_ms=" << msSubmitScene << " resize_ms=" << msResize
							<< " end_frame_callback_ms=" << msEndFrameCb << " end_game_thread_sync_ms=" << msGpuSync << " tick_total_ms=" << msTickTotal << "\n";
			}
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
