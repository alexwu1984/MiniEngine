#pragma once

#include "core/inc.h"
#include "core/event.h"

namespace core
{
	class timer
	{
	public:
        timer();
		timer(std::chrono::milliseconds period);
		~timer();

		timer(const timer & another) = delete;

		void start();
		void start(std::chrono::milliseconds period);
		void stop();

		void callback();

	public:
        core::event<void(timer &, int64_t)> tick;

	private:
        std::shared_ptr<void> _context;
		std::atomic<bool> _started = false;
		std::chrono::milliseconds _period = 1000ms;
		int64_t _tick = 0;
	};

	inline double_t get_time_hr()
	{
		auto now = std::chrono::high_resolution_clock::now();
		auto nowmcs = std::chrono::duration_cast<std::chrono::microseconds>(now.time_since_epoch()).count();
		return nowmcs / 1000000.0;
	}

	inline double_t get_time_now_ms()
	{
		return get_time_hr() * 1000;
	}
}
