#include "RHI/RHIThreadPolicy.h"
#include <mutex>
#include <optional>

namespace RenderCore
{
	namespace
	{
		std::mutex GSubmissionThreadMutex;
		std::optional<std::thread::id> GSubmissionThreadId;
	}

	void RHI_RegisterRHISubmissionThread(std::thread::id ThreadId)
	{
		std::lock_guard<std::mutex> Lock(GSubmissionThreadMutex);
		GSubmissionThreadId = ThreadId;
	}

	void RHI_UnregisterRHISubmissionThread()
	{
		std::lock_guard<std::mutex> Lock(GSubmissionThreadMutex);
		GSubmissionThreadId.reset();
	}

	bool RHI_IsRHISubmissionThreadRegistered()
	{
		std::lock_guard<std::mutex> Lock(GSubmissionThreadMutex);
		return GSubmissionThreadId.has_value();
	}

	bool RHI_IsInRHISubmissionThread()
	{
		std::lock_guard<std::mutex> Lock(GSubmissionThreadMutex);
		if (!GSubmissionThreadId.has_value())
			return true;
		return std::this_thread::get_id() == *GSubmissionThreadId;
	}
}
