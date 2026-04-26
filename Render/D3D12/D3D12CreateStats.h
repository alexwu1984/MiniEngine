#pragma once

#include <atomic>
#include <cstdint>

namespace RenderCore::D3D12CreateStats
{
	// Tracks D3D12 creates that bypass FD3D12Resource (e.g. linear allocator pages).
	// Keep this header-only to avoid link/dependency churn.

	inline std::atomic_uint64_t& LinearPage_CreateCount_Default()
	{
		static std::atomic_uint64_t v{0};
		return v;
	}
	inline std::atomic_uint64_t& LinearPage_CreateBytes_Default()
	{
		static std::atomic_uint64_t v{0};
		return v;
	}
	inline std::atomic_uint64_t& LinearPage_CreateCount_Upload()
	{
		static std::atomic_uint64_t v{0};
		return v;
	}
	inline std::atomic_uint64_t& LinearPage_CreateBytes_Upload()
	{
		static std::atomic_uint64_t v{0};
		return v;
	}
}

