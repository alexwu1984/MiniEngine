#include "Engine/Thread/RHISubmissionThread.h"
#include "Engine/ComErrorLog.h"
#include "D3D12/D3D12RHIRecording.h"
#include "RHI/DynamicRHI.h"
#include "RHI/RHIThreadPolicy.h"
#include <future>

namespace Engine
{
	RHISubmissionThread::RHISubmissionThread() = default;

	RHISubmissionThread::~RHISubmissionThread()
	{
		Stop();
	}

	void RHISubmissionThread::Start()
	{
		if (Worker.joinable())
			return;
		StopFlag = false;
		Worker = std::thread([this]() { RunLoop(); });
	}

	void RHISubmissionThread::Stop()
	{
		{
			std::lock_guard<std::mutex> Lock(QueueMutex);
			StopFlag = true;
		}
		QueueCv.notify_all();
		if (Worker.joinable())
			Worker.join();
		StopFlag = false;
	}

	void RHISubmissionThread::EnqueueAndWait(std::function<void()> Work)
	{
		if (!Work)
			return;
		auto Done = std::make_shared<std::promise<void>>();
		std::future<void> Fut = Done->get_future();
		{
			std::lock_guard<std::mutex> Lock(QueueMutex);
			Queue.push_back([Work = std::move(Work), Done]() {
				auto finish = [Done]() { Done->set_value(); };
				if (RenderCore::RHI_HasFatalDeviceLossForShell())
				{
					finish();
					return;
				}
				try
				{
					RenderCore::D3D12RHI_ScopedExclusiveRegion SubmitScope;
					Work();
				}
				catch (const _com_error& e)
				{
					LogComErrorToEngineLog(L"RHISubmissionThread::EnqueueAndWait", e);
				}
				catch (const std::exception& e)
				{
					LogStdExceptionToEngineLog(L"RHISubmissionThread::EnqueueAndWait", e);
				}
				catch (...)
				{
					LogUnknownExceptionToEngineLog(L"RHISubmissionThread::EnqueueAndWait");
				}
				finish();
			});
		}
		QueueCv.notify_one();
		Fut.wait();
	}

	void RHISubmissionThread::RunLoop()
	{
		RenderCore::RHI_RegisterRHIExecutionThread(std::this_thread::get_id());
		struct UnregisterOnExit
		{
			~UnregisterOnExit()
			{
				RenderCore::RHI_UnregisterRHIExecutionThread();
			}
		} UnregisterScope;

		for (;;)
		{
			std::function<void()> Task;
			{
				std::unique_lock<std::mutex> Lock(QueueMutex);
				QueueCv.wait(Lock, [this]() { return StopFlag.load() || !Queue.empty(); });
				if (StopFlag && Queue.empty())
					break;
				Task = std::move(Queue.front());
				Queue.pop_front();
			}
			if (Task)
				Task();
		}
	}
}
