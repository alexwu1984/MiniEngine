#include "Engine/Thread/RenderThread.h"

namespace Engine
{
	RenderThread* GRenderThread = nullptr;
	struct RenderThreadP
	{
		std::queue<std::function<void()>> _CmdQueue;
		std::mutex _Lock;
		std::mutex _DataLock;
		std::thread _thread;
		std::condition_variable _notify;
		bool _Stop = false;
		std::thread::id _RenderThreadId = {};
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
		if (!Data->_thread.joinable())
		{
			Data->_thread = std::thread(&RenderThread::Run, this);
			Data->_Stop = false;
		}
	}

	void RenderThread::Stop()
	{
		if (Data->_thread.joinable())
		{
			Data->_Stop = true;
			Data->_notify.notify_one();
			Data->_thread.join();
		}
	}

	void RenderThread::AppendCommand(std::function<void()> fun)
	{
		if (!fun)
		{
			return;
		}

		if (std::this_thread::get_id() == Data->_RenderThreadId)
		{
			fun();
		}
		else
		{
			{
				std::unique_lock<std::mutex> lock(Data->_Lock);
				Data->_CmdQueue.push(fun);
			}
			Data->_notify.notify_one();
		}

	}


	void RenderThread::Run()
	{
		Data->_RenderThreadId = std::this_thread::get_id();
		while (!Data->_Stop)
		{
			{
				std::unique_lock<std::mutex> ConLock(Data->_Lock);
				Data->_notify.wait(ConLock, [this]() {
					return Data->_Stop || !Data->_CmdQueue.empty();
					});
			}

			std::queue<std::function<void()>> SwapCmd;
			{
				std::unique_lock<std::mutex> ConLock(Data->_Lock);
				Data->_CmdQueue.swap(SwapCmd);
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

		}
	}

}