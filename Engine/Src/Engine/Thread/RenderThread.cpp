#include "Engine/Thread/RenderThread.h"
#include "win/sync.h"
#include "RHI/DynamicRHI.h"

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

	}


	void RenderThread::Start()
	{
		if (!Impl->Thread.joinable())
		{
			Impl->WaitForFinish.create(true, true);
			Impl->Thread = std::thread(&RenderThread::Run, this);
			Impl->Stop = false;
		}
	}

	void RenderThread::Stop()
	{
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
			{
				std::unique_lock<std::mutex> ConLock(Impl->CondLock);
				Impl->Notify.wait(ConLock, [this]() {
					return Impl->Stop || !Impl->CmdQueue.empty();
					});
			}

			Impl->WaitForFinish.reset();

			std::queue<std::function<void(RenderCore::DynamicRHI*)>> SwapCmd;
			{
				std::unique_lock<std::mutex> ConLock(Impl->CondLock);
				Impl->CmdQueue.swap(SwapCmd);
			}
			
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

}