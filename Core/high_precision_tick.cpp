#include "win/high_precision_tick.h"
#include "win/cpu_clock.h"
#include "core/logger.h"

namespace win32
{
	struct HighPrecisionTickPrivate
	{
		HANDLE _hThread = nullptr;
		int32_t _Fps = 60;
		std::atomic_bool _Quit = false;
	};

	void UpdatePublishFPS(uint32_t& fpsFrameNum, uint64_t& fpsBeginTime, const std::wstring& tips, int statisticTime)
	{
		if (0 == fpsBeginTime)
		{
			fpsFrameNum = 0;
			fpsBeginTime = win32::cpu_clock::os_gettime_ms();
			return;
		}

		fpsFrameNum++;
		uint64_t interval = win32::cpu_clock::os_gettime_ms() - fpsBeginTime;
		if ((int)interval > statisticTime)
		{
			int fps = static_cast<int32_t>(fpsFrameNum * 1000 / interval);
			//GLog(TRACE_INFO, __FUNCTION__ "  %s AVG_FPS : %d FPS : %d", tips.c_str(), fps, fpsFrameNum);
			core::LOG(core::log_inf, L"  %s AVG_FPS : %d FPS : %d", tips.c_str(), fps, fpsFrameNum);

			fpsFrameNum = 0;
			fpsBeginTime = win32::cpu_clock::os_gettime_ms();
		}
	}

	static void UpdateRenderFPS(const std::wstring& tips)
	{
		static uint32_t fpsFrameNum = 0;
		static uint64_t fpsBeginTime = 0;
		UpdatePublishFPS(fpsFrameNum, fpsBeginTime, tips, 5 * 1000);
	}

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
		core::LOG(core::log_inf, __FUNCTIONW__ L" Name:%s fps=%d", core::u8_ucs2(Name).c_str(), Fps);
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

		uint64_t LastTickTimeNS = win32::cpu_clock::os_gettime_ns();

		while (!Impl->_Quit.load())
		{
			win32::cpu_clock::os_sleepto_ns(&SleepTargetTime, FrameTimeNS);
			if (Impl->_Quit.load())
			{
				break;
			}

			uint64_t CurrentTimeNS = win32::cpu_clock::os_gettime_ns();
			uint64_t DeltaNS = CurrentTimeNS - LastTickTimeNS;
			float Delta = static_cast<float>(DeltaNS) / 1000000000.0f;
			LastTickTimeNS = CurrentTimeNS;

			UpdateRenderFPS(L"=====Tick=====");
			auto TStart = std::chrono::high_resolution_clock::now();
			SigTick(Delta);
			auto TEnd = std::chrono::high_resolution_clock::now();
			float CostTime = std::chrono::duration<float, std::milli>(TEnd - TStart).count();
			if (CostTime > 5)
			{
				core::LOG(core::log_inf, __FUNCTIONW__ L" Too Long:%0.1fms", CostTime);
			}
			
		}
		core::LOG(core::log_inf, __FUNCTIONW__ L" Quit");
	}

}