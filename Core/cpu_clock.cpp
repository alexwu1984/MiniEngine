#include "win/cpu_clock.h"

namespace win32 
{
	static bool have_clockfreq = false;
	static LARGE_INTEGER clock_freq;

	static inline uint64_t get_clockfreq(void)
	{
		if (!have_clockfreq) {
			QueryPerformanceFrequency(&clock_freq);
			have_clockfreq = true;
		}

		return clock_freq.QuadPart;
	}

	bool cpu_clock::os_sleepto_ns(uint64_t time_target)
	{
		uint64_t t = os_gettime_ns();
		uint32_t milliseconds;

		if (t >= time_target)
			return false;

		milliseconds = (uint32_t)((time_target - t) / 1000000);
		if (milliseconds > 1)
			Sleep(milliseconds - 1);

		for (;;) {
			t = os_gettime_ns();
			if (t >= time_target)
				return true;

#if 0
			Sleep(1);
#else
			Sleep(0);
#endif
		}
	}


	void cpu_clock::os_sleep_ms(uint32_t duration)
	{
		/* windows 8+ appears to have decreased sleep precision */
		if (win32::version() >= win32::winversion_8 && duration > 0)
			duration--;

		Sleep(duration);
	}

	int cpu_clock::os_sleepto_ns(uint64_t* p_time, uint64_t interval_ns)
	{
		uint64_t cur_time = *p_time;
		uint64_t t = cur_time + interval_ns;
		int count;

		if (os_sleepto_ns(t))
		{
			*p_time = t;
			count = 1;
		}
		else
		{
			const uint64_t udiff = os_gettime_ns() - cur_time;
			int64_t diff;
			memcpy(&diff, &udiff, sizeof(diff));
			const uint64_t clamped_diff = (diff > (int64_t)interval_ns)
				? (uint64_t)diff
				: interval_ns;
			count = (int)(clamped_diff / interval_ns);
			*p_time = cur_time + interval_ns * count;
		}

		return count;
	}

	uint64_t cpu_clock::os_gettime_ns(void)
	{
		LARGE_INTEGER current_time;
		double time_val;

		QueryPerformanceCounter(&current_time);
		time_val = (double)current_time.QuadPart;
		time_val *= 1000000000.0;
		time_val /= (double)get_clockfreq();

		return (uint64_t)time_val;
	}

	uint64_t cpu_clock::os_gettime_ms(void)
	{
		LARGE_INTEGER current_time;
		double time_val;

		QueryPerformanceCounter(&current_time);
		time_val = (double)current_time.QuadPart;
		time_val *= 1000.0;
		time_val /= (double)get_clockfreq();

		return (uint64_t)time_val;
	}
}