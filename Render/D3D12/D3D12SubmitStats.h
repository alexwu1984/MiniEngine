#pragma once

#include <atomic>
#include <cstdint>

namespace RenderCore::D3D12SubmitStats
{
	// Counts command queue submissions (ExecuteCommandLists + Signal) per queue type.
	// Used to detect "too many submits per frame" patterns that can balloon driver/DXGI memory.

	inline std::atomic_uint64_t& SubmitCount_Direct()
	{
		static std::atomic_uint64_t v{0};
		return v;
	}
	inline std::atomic_uint64_t& SubmitCount_Copy()
	{
		static std::atomic_uint64_t v{0};
		return v;
	}
	inline std::atomic_uint64_t& SubmitCount_Compute()
	{
		static std::atomic_uint64_t v{0};
		return v;
	}
}

