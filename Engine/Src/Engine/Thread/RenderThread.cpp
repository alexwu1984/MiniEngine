#include "Engine/Thread/RenderThread.h"
#include "win/sync.h"
#include "RHI/DynamicRHI.h"
#include "core/logger.h"

namespace Engine
{
	RenderThread* GRenderThread = nullptr;

	struct RenderThreadP
	{
		std::queue<std::function<void(RenderCore::DynamicRHI*)>> CmdQueue;
		std::mutex CondLock;
		std::thread Thread;
		std::condition_variable Notify;
		win32::signal WaitForFinish;
		bool Stop = false;
		std::thread::id RenderThreadId = {};
		RenderCore::DynamicRHI* DyRHI = nullptr;
	};

	RenderThread::RenderThread(RenderCore::DynamicRHI* DyRHI)
		:Impl(std::make_unique<RenderThreadP>())
	{
		GRenderThread = this;
		Impl->DyRHI = DyRHI;
	}

	RenderThread::~RenderThread()
	{
		GRenderThread = nullptr;
	}


	void RenderThread::Start()
	{
		LOG(core::log_inf, __FUNCTIONW__);
		if (!Impl->Thread.joinable())
		{
			Impl->WaitForFinish.create(true, true);
			Impl->Thread = std::thread(&RenderThread::Run, this);
			Impl->Stop = false;
		}
	}

	void RenderThread::Stop()
	{
		LOG(core::log_inf, __FUNCTIONW__);
		if (Impl->Thread.joinable())
		{
			Impl->Stop = true;
			Impl->Notify.notify_one();
			Impl->Thread.join();
		}
	}

	void RenderThread::AppendCommand(std::function<void(RenderCore::DynamicRHI*)> Fun)
	{
		if (!Fun)
		{
			return;
		}

		if (std::this_thread::get_id() == Impl->RenderThreadId)
		{
			Fun(Impl->DyRHI);
		}
		else
		{
			{
				std::unique_lock<std::mutex> lock(Impl->CondLock);
				Impl->CmdQueue.push(Fun);
			}
			Impl->Notify.notify_one();
		}
	}

	void RenderThread::WaitForFinish()
	{
		Impl->WaitForFinish.wait();
	}

	void RenderThread::Run()
	{
		Impl->RenderThreadId = std::this_thread::get_id();
		while (!Impl->Stop)
		{
			std::queue<std::function<void(RenderCore::DynamicRHI*)>> SwapCmd;
			{
				std::unique_lock<std::mutex> ConLock(Impl->CondLock);
				Impl->Notify.wait(ConLock, [this]() {
					return Impl->Stop || !Impl->CmdQueue.empty();
					});
				Impl->CmdQueue.swap(SwapCmd);
			}

			if (Impl->Stop)
			{	
				Impl->WaitForFinish.set();
				break;
			}

			Impl->WaitForFinish.reset();
			
			while (!SwapCmd.empty())
			{
				auto& Cmd = SwapCmd.front();
				if (Cmd)
				{
					Cmd(Impl->DyRHI);
				}
				SwapCmd.pop();
			}

			Impl->WaitForFinish.set();
		}
	}

	void ENQUEUE_UNIQUE_RENDER_COMMAND(std::function<void(RenderCore::DynamicRHI*)> fun,bool wait)
	{
		if (GRenderThread)
		{
			GRenderThread->AppendCommand(fun);
			if (wait)
			{
				GRenderThread->WaitForFinish();
			}
		}
	}

}