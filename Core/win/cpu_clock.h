#pragma once
#include "win/win32.h"

namespace win32
{
	class cpu_clock
	{
	public:
		static bool os_sleepto_ns(uint64_t time_target);
		static int os_sleepto_ns(uint64_t* p_time, uint64_t interval_ns);
		static void os_sleep_ms(uint32_t duration);

		static uint64_t os_gettime_ns(void);
		static uint64_t os_gettime_ms(void);
	};
}