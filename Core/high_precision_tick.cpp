#include "win/high_precision_tick.h"
#include "win/cpu_clock.h"
#include "core/logger.h"

namespace win32
{
	struct HighPrecisionTickPrivate
	{
		HANDLE _hThread = nullptr;
		int32_t _Fps = 60;
		bool _Quit = false;
	};

	HighPrecisionTick::HighPrecisionTick()
		:Impl(new HighPrecisionTickPrivate())
	{

	}

	HighPrecisionTick::~HighPrecisionTick()
	{
		delete Impl;
	}

	void HighPrecisionTick::Start(const std::string& Name, int32_t Fps, ThreadPriority Priority /*= ThreadPriority::Normal*/)
	{
		core::LOG(core::log_inf, __FUNCTIONW__ L" Name:%s fps=%d", Name.c_str(), Fps);
		Impl->_Fps = Fps;
		Impl->_Quit = false;
		Impl->_hThread = (HANDLE)_beginthreadex(nullptr, 0, [](void* Param)->uint32_t {
			HighPrecisionTick* pThis = (HighPrecisionTick*)Param;
			pThis->InnerThread();
			return 0;
		},this, CREATE_SUSPENDED, nullptr);
		if (!Impl->_hThread)
		{
			core::LOG(core::log_inf, __FUNCTIONW__ L" _beginthreadex failed");
		}
	
		switch (Priority)
		{
		case ThreadPriority::Highest:
			::SetThreadPriority(Impl->_hThread, THREAD_PRIORITY_HIGHEST);
			break;
		case ThreadPriority::Normal:
			::SetThreadPriority(Impl->_hThread, THREAD_PRIORITY_NORMAL);
			break;
		case ThreadPriority::BelowNormal:
			::SetThreadPriority(Impl->_hThread, THREAD_PRIORITY_BELOW_NORMAL);
			break;
		}

		::ResumeThread(Impl->_hThread);
	}

	void HighPrecisionTick::Stop()
	{
		core::LOG(core::log_inf, __FUNCTIONW__);

		if (Impl->_hThread)
		{
			Impl->_Quit = true;
			if (WAIT_TIMEOUT == ::WaitForSingleObject(Impl->_hThread, 3000))
			{
				core::LOG(core::log_inf, __FUNCTIONW__ L" Quit too slow!!");
			}
			CloseHandle(Impl->_hThread);
			Impl->_hThread = nullptr;
		}
	}

	void HighPrecisionTick::InnerThread()
	{
		core::LOG(core::log_inf, __FUNCTIONW__);

		uint64_t FrameTimeNS = 1000000000 / Impl->_Fps;
		uint64_t SleepTargetTime = win32::cpu_clock::os_gettime_ns();

		std::chrono::high_resolution_clock::time_point TStart = std::chrono::high_resolution_clock::now();
		std::chrono::high_resolution_clock::time_point TEnd;
		
		while (!Impl->_Quit)
		{
			win32::cpu_clock::os_sleepto_ns(&SleepTargetTime, FrameTimeNS);
			if (Impl->_Quit)
			{
				break;
			}
			TEnd = std::chrono::high_resolution_clock::now();
			float Delta = std::chrono::duration<float, std::milli>(TEnd - TStart).count();
			sigTick(Delta);
			TStart = std::chrono::high_resolution_clock::now();
		}
		core::LOG(core::log_inf, __FUNCTIONW__ L" Quit");
	}

}