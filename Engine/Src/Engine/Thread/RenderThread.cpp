#include "Engine/Thread/RenderThread.h"
#include "Engine/ComErrorLog.h"
#include "Render/RenderQueueSynchronization.h"
#include "win/sync.h"
#include "RHI/DynamicRHI.h"
#include "RHI/RHIThreadPolicy.h"
#include "core/logger.h"

namespace Engine
{
	namespace
	{
		static void InvokeRenderQueueLambda(RenderCore::DynamicRHI* OwnerRHI,
											std::function<void(RenderCore::DynamicRHI*)>& Cmd,
											const wchar_t* ContextTag)
		{
			if (!Cmd)
				return;
			try
			{
				if (!OwnerRHI || !RenderCore::RHI_HasFatalDeviceLossForShell())
					Cmd(OwnerRHI);
			}
			catch (const _com_error& e)
			{
				LogComErrorToEngineLog(ContextTag, e);
			}
			catch (const std::exception& e)
			{
				LogStdExceptionToEngineLog(ContextTag, e);
			}
			catch (...)
			{
				LogUnknownExceptionToEngineLog(ContextTag);
			}
		}

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

	void RenderThread::PumpRecordingQueueUntilEmpty()
	{
		C_P(RenderThread);
		Assert(std::this_thread::get_id() == d->RecordingThreadId);
		static constexpr wchar_t kCtx[] = L"RenderThread::PumpRecordingQueueUntilEmpty";
		for (;;)
		{
			std::queue<std::function<void(RenderCore::DynamicRHI*)>> slice;
			{
				std::unique_lock<std::mutex> lock(d->QueueMutex);
				if (d->CommandQueue.empty())
					return;
				d->CommandQueue.swap(slice);
			}
			while (!slice.empty())
			{
				auto& cmd = slice.front();
				InvokeRenderQueueLambda(d->OwnerRHI, cmd, kCtx);
				slice.pop();
			}
		}
	}

	void RenderThread::WaitForFinish()
	{
		C_P(RenderThread);
		// After Stop() the worker is joined; DrainWait is never set again — waiting would deadlock
		// (e.g. FWorldSceneRender dtor runs after Engine::ShutDown already stopped the render thread).
		if (!d->WorkerThread.joinable())
			return;
		// On the recording worker: never wait on DrainWait — it only signals after the outer batch ends.
		// Drain cross-thread CommandQueue items immediately so Flush actually synchronizes nested/recording-thread callers.
		if (std::this_thread::get_id() == d->RecordingThreadId)
		{
			PumpRecordingQueueUntilEmpty();
			return;
		}
		// Loop: one DrainWait pulse completes one swap-batch; the worker may immediately start another batch
		// (same Flush must not return until queue is drained and worker is idle — Release timing exposed this gap).
		for (;;)
		{
			{
				std::unique_lock<std::mutex> lock(d->QueueMutex);
				if (!d->bExecutingBatch.load(std::memory_order_acquire) && d->CommandQueue.empty())
					return;
			}
			d->DrainWait.wait();
		}
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

			static constexpr wchar_t kRunBatchCtx[] = L"RenderThread::Run(worker_batch)";
			while (!swapQueue.empty())
			{
				auto& cmd = swapQueue.front();
				InvokeRenderQueueLambda(d->OwnerRHI, cmd, kRunBatchCtx);
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
