#include "Engine/Thread/RenderThread.h"
#include "Engine/ComErrorLog.h"
#include "Render/RenderQueueSynchronization.h"
#include "win/sync.h"
#include "RHI/DynamicRHI.h"
#include "RHI/RHIThreadPolicy.h"
#include "core/logger.h"
#include <atomic>

namespace Engine
{
	namespace
	{
#if defined(MINIENGINE_RENDER_QUEUE_FLUSH_AUDIT)
		void AuditRenderQueueFlush(ERenderQueueFlushCategory Category)
		{
			core::inf() << "RenderFlush category=" << static_cast<unsigned>(Category);
		}
#else
		void AuditRenderQueueFlush(ERenderQueueFlushCategory) {}
#endif

		void FlushRecordingWorkerDrain()
		{
			if (GRenderThread)
				GRenderThread->WaitForFinish();
		}
	} // namespace

	void FlushRenderingCommands()
	{
		FlushRecordingWorkerDrain();
	}

	void FlushRenderingCommands(ERenderQueueFlushCategory Category)
	{
		AuditRenderQueueFlush(Category);
		FlushRecordingWorkerDrain();
	}

	RenderThread* GRenderThread = nullptr;

	struct RenderThreadPrivate
	{
		std::queue<std::function<void(RenderCore::DynamicRHI*)>> CommandQueue;
		std::mutex QueueMutex;
		std::condition_variable QueueWakeup;
		std::thread WorkerThread;
		win32::signal DrainWait;
		bool bStopRequested = false;
		std::atomic<bool> bExecutingBatch{false};
		std::thread::id RecordingThreadId = {};
		RenderCore::DynamicRHI* OwnerRHI = nullptr;
	};

	RenderThread::RenderThread(RenderCore::DynamicRHI* DyRHI)
		: d_ptr(new RenderThreadPrivate())
	{
		GRenderThread = this;
		C_P(RenderThread);
		d->OwnerRHI = DyRHI;
	}

	RenderThread::~RenderThread()
	{
		GRenderThread = nullptr;
		delete d_ptr;
		d_ptr = nullptr;
	}

	void RenderThread::Start()
	{
		LOG(core::log_inf, __FUNCTIONW__);
		C_P(RenderThread);
		if (!d->WorkerThread.joinable())
		{
			// Auto-reset + initially non-signaled: WaitForSingleObject clears the signal.
			// Manual-reset + initially set caused WaitForFinish() to return before the queued batch ran (game/render race).
			d->DrainWait.create(false, false);
			d->WorkerThread = std::thread(&RenderThread::Run, this);
			d->bStopRequested = false;
		}
	}

	void RenderThread::Stop()
	{
		LOG(core::log_inf, __FUNCTIONW__);
		C_P(RenderThread);
		if (d->WorkerThread.joinable())
		{
			// Ensure all queued render commands are executed before stopping.
			// Otherwise, commands captured by value (e.g. shared_ptr textures/gbuffer)
			// can keep GPU resources alive until process exit.
			AppendCommand([](RenderCore::DynamicRHI*) {});
			FlushRenderingCommands(ERenderQueueFlushCategory::LifetimeOrShutdown);

			// Drop any remaining queued commands to release captured resources.
			{
				std::unique_lock<std::mutex> lock(d->QueueMutex);
				while (!d->CommandQueue.empty())
					d->CommandQueue.pop();
			}

			d->bStopRequested = true;
			d->QueueWakeup.notify_one();
			d->WorkerThread.join();
		}
	}

	void RenderThread::AppendCommand(std::function<void(RenderCore::DynamicRHI*)> Fun)
	{
		if (!Fun)
		{
			return;
		}

		C_P(RenderThread);
		if (std::this_thread::get_id() == d->RecordingThreadId)
		{
			try
			{
				Fun(d->OwnerRHI);
			}
			catch (const _com_error& e)
			{
				LogComErrorToEngineLog(L"RenderThread::AppendCommand(inline_recording_thread)", e);
			}
			catch (const std::exception& e)
			{
				LogStdExceptionToEngineLog(L"RenderThread::AppendCommand(inline_recording_thread)", e);
			}
			catch (...)
			{
				LogUnknownExceptionToEngineLog(L"RenderThread::AppendCommand(inline_recording_thread)");
			}
		}
		else
		{
			{
				std::unique_lock<std::mutex> lock(d->QueueMutex);
				d->CommandQueue.push(std::move(Fun));
			}
			d->QueueWakeup.notify_one();
		}
	}

	void RenderThread::WaitForFinish()
	{
		C_P(RenderThread);
		// After Stop() the worker is joined; DrainWait is never set again — waiting would deadlock
		// (e.g. FWorldSceneRender dtor runs after Engine::ShutDown already stopped the render thread).
		if (!d->WorkerThread.joinable())
			return;
		// Nested ENQUEUE(..., wait=true) executes lambdas inline on the worker; waiting here would block
		// forever because this batch's DrainWait.set() runs only after the outer cmd returns.
		if (std::this_thread::get_id() == d->RecordingThreadId)
			return;
		// DrainWait is auto-reset: the first WaitForFinish() consumes the signal while the worker is idle
		// on QueueWakeup (no further DrainWait.set() until another batch runs). A second WaitForFinish()
		// on the same idle state — e.g. ReloadSceneJson then SceneMeshComponent::~SceneMeshComponent —
		// would hang forever without this fast-path.
		{
			std::unique_lock<std::mutex> lock(d->QueueMutex);
			if (!d->bExecutingBatch.load(std::memory_order_acquire) && d->CommandQueue.empty())
				return;
		}
		d->DrainWait.wait();
	}

	std::thread::id RenderThread::GetWorkerThreadId() const
	{
		C_P(const RenderThread);
		if (!d->WorkerThread.joinable())
			return {};
		return d->WorkerThread.get_id();
	}

	void RenderThread::Run()
	{
		C_P(RenderThread);
		d->RecordingThreadId = std::this_thread::get_id();
		RenderCore::RHI_RegisterRHIRecordingThread(d->RecordingThreadId);
		struct UnregisterRecordingThread
		{
			~UnregisterRecordingThread()
			{
				RenderCore::RHI_UnregisterRHIRecordingThread();
			}
		} UnregisterRecordingOnScopeExit;

		while (!d->bStopRequested)
		{
			std::queue<std::function<void(RenderCore::DynamicRHI*)>> swapQueue;
			{
				std::unique_lock<std::mutex> lock(d->QueueMutex);
				d->QueueWakeup.wait(lock, [this]() {
					return d_ptr->bStopRequested || !d_ptr->CommandQueue.empty();
				});
				d->CommandQueue.swap(swapQueue);
				// Reset while still holding QueueMutex so a concurrent WaitForFinish() cannot observe the
				// previous batch's signaled event after we've dequeued work but before reset() runs.
				if (!swapQueue.empty() && !d->bStopRequested)
				{
					d->DrainWait.reset();
					d->bExecutingBatch.store(true, std::memory_order_release);
				}
			}

			if (d->bStopRequested)
			{
				d->bExecutingBatch.store(false, std::memory_order_release);
				d->DrainWait.set();
				break;
			}

			while (!swapQueue.empty())
			{
				auto& cmd = swapQueue.front();
				if (cmd)
				{
					try
					{
						if (!d->OwnerRHI || !RenderCore::RHI_HasFatalDeviceLossForShell())
							cmd(d->OwnerRHI);
					}
					catch (const _com_error& e)
					{
						LogComErrorToEngineLog(L"RenderThread::Run(worker_batch)", e);
					}
					catch (const std::exception& e)
					{
						LogStdExceptionToEngineLog(L"RenderThread::Run(worker_batch)", e);
					}
					catch (...)
					{
						LogUnknownExceptionToEngineLog(L"RenderThread::Run(worker_batch)");
					}
				}
				swapQueue.pop();
			}

			d->bExecutingBatch.store(false, std::memory_order_release);
			d->DrainWait.set();
		}
	}

	void ENQUEUE_UNIQUE_RENDER_COMMAND(std::function<void(RenderCore::DynamicRHI*)> fun, bool wait)
	{
		if (GRenderThread)
		{
			GRenderThread->AppendCommand(std::move(fun));
			if (wait)
			{
				GRenderThread->WaitForFinish();
			}
		}
	}

}
