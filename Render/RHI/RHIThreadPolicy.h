#pragma once
#include <thread>

namespace RenderCore
{
	/**
	 * UE-style RHI submission thread identity (MVP: same as Engine RenderThread).
	 * When no thread is registered (e.g. DemoRunner / tools), submission-thread checks are skipped.
	 */
	void RHI_RegisterRHISubmissionThread(std::thread::id ThreadId);
	void RHI_UnregisterRHISubmissionThread();
	bool RHI_IsRHISubmissionThreadRegistered();

	/** True if no submission thread is registered, or current thread matches the registered id. */
	bool RHI_IsInRHISubmissionThread();

	/** Alias for UE naming; same as RHI_IsInRHISubmissionThread(). */
	inline bool IsInRHIThread()
	{
		return RHI_IsInRHISubmissionThread();
	}
}
