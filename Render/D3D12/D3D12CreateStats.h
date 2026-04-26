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

	// Linear allocator lifecycle (for correlating VMem WC deltas with engine paths; deltas in memmon tick).
	inline std::atomic_uint64_t& LinearPage_UploadLargeCreateCount()
	{
		static std::atomic_uint64_t v{0};
		return v;
	}
	inline std::atomic_uint64_t& LinearPage_UploadLargeCreateBytes()
	{
		static std::atomic_uint64_t v{0};
		return v;
	}
	inline std::atomic_uint64_t& LinearPage_ReuseFromReadyCount()
	{
		static std::atomic_uint64_t v{0};
		return v;
	}
	inline std::atomic_uint64_t& LinearPage_FenceWaitReuseCount()
	{
		static std::atomic_uint64_t v{0};
		return v;
	}
	inline std::atomic_uint64_t& LinearPage_DiscardStandardPageCount()
	{
		static std::atomic_uint64_t v{0};
		return v;
	}
	inline std::atomic_uint64_t& LinearPage_StandardCacheReleaseCount()
	{
		static std::atomic_uint64_t v{0};
		return v;
	}
	inline std::atomic_uint64_t& LinearPage_LargePageDestroyedCount()
	{
		static std::atomic_uint64_t v{0};
		return v;
	}
}

