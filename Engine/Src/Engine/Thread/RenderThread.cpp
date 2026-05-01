#include "Engine/Thread/RenderThread.h"
#include "win/sync.h"
#include "RHI/DynamicRHI.h"
#include "RHI/RHIThreadPolicy.h"
#include "core/logger.h"
#include <thread>

namespace Engine
{
	RenderThread* GRenderThread = nullptr;

	struct RenderThreadPrivate
	{
		std::queue<std::function<void(RenderCore::DynamicRHI*)>> CommandQueue;
		std::mutex QueueMutex;
		std::condition_variable QueueWakeup;
		std::thread WorkerThread;
		win32::signal DrainWait;
		bool bStopRequested = false;
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
			WaitForFinish();

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
			Fun(d->OwnerRHI);
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
			}

			if (d->bStopRequested)
			{
				d->DrainWait.set();
				break;
			}

			d->DrainWait.reset();

			while (!swapQueue.empty())
			{
				auto& cmd = swapQueue.front();
				if (cmd)
				{
					cmd(d->OwnerRHI);
				}
				swapQueue.pop();
			}

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
