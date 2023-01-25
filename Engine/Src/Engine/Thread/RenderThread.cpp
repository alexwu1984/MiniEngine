#include "Engine/Thread/RenderThread.h"
#include "win/sync.h"

namespace Engine
{
	RenderThread* GRenderThread = nullptr;
	struct RenderThreadP
	{
		std::queue<std::function<void()>> CmdQueue;
		std::mutex CondLock;
		std::thread Thread;
		std::condition_variable Notify;
		win32::signal WaitForFinish;
		bool Stop = false;
		std::thread::id RenderThreadId = {};
	};

	RenderThread::RenderThread()
		:Data(std::make_unique<RenderThreadP>())
	{
		GRenderThread = this;
	}

	RenderThread::~RenderThread()
	{

	}


	void RenderThread::Start()
	{
		if (!Data->Thread.joinable())
		{
			Data->WaitForFinish.create(true, true);
			Data->Thread = std::thread(&RenderThread::Run, this);
			Data->Stop = false;
		}
	}

	void RenderThread::Stop()
	{
		if (Data->Thread.joinable())
		{
			Data->Stop = true;
			Data->Notify.notify_one();
			Data->Thread.join();
		}
	}

	void RenderThread::AppendCommand(std::function<void()> fun)
	{
		if (!fun)
		{
			return;
		}

		if (std::this_thread::get_id() == Data->RenderThreadId)
		{
			fun();
		}
		else
		{
			{
				std::unique_lock<std::mutex> lock(Data->CondLock);
				Data->CmdQueue.push(fun);
			}
			Data->Notify.notify_one();
		}

	}


	void RenderThread::WaitForFinish()
	{
		Data->WaitForFinish.wait();
	}

	void RenderThread::Run()
	{
		Data->RenderThreadId = std::this_thread::get_id();
		while (!Data->Stop)
		{
			{
				std::unique_lock<std::mutex> ConLock(Data->CondLock);
				Data->Notify.wait(ConLock, [this]() {
					return Data->Stop || !Data->CmdQueue.empty();
					});
			}

			Data->WaitForFinish.reset();

			std::queue<std::function<void()>> SwapCmd;
			{
				std::unique_lock<std::mutex> ConLock(Data->CondLock);
				Data->CmdQueue.swap(SwapCmd);
			}
			
			while (!SwapCmd.empty())
			{
				auto& Cmd = SwapCmd.front();
				if (Cmd)
				{
					Cmd();
				}
				SwapCmd.pop();
			}

			Data->WaitForFinish.set();
		}
	}

}