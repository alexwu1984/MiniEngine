#pragma once

#include <atomic>
#include <cstdint>

// Present / window visibility counters for d3d12_memmon (RuntimeStatsMonitor).
// Increments only when command line enables d3d12_memmon (see D3D12ViewPort::Present).

namespace RenderCore::D3D12PresentStats
{
	inline std::atomic_uint64_t& PresentCalls()
	{
		static std::atomic_uint64_t v{0};
		return v;
	}

	inline std::atomic_uint64_t& PresentOccluded()
	{
		static std::atomic_uint64_t v{0};
		return v;
	}

	inline std::atomic_uint64_t& PresentFailed()
	{
		static std::atomic_uint64_t v{0};
		return v;
	}

	inline std::atomic_uint64_t& WindowIconic()
	{
		static std::atomic_uint64_t v{0};
		return v;
	}

	inline std::atomic_uint64_t& WindowNotVisible()
	{
		static std::atomic_uint64_t v{0};
		return v;
	}
}

