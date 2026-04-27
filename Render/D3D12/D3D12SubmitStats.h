#pragma once

#include "RHI/RHI.h"
#include <atomic>
#include <cstdint>

namespace RenderCore
{
	enum class ED3D12CommandQueueType;
}

namespace RenderCore::D3D12SubmitStats
{
	// Counts command queue submissions (ExecuteCommandLists + Signal) per queue type.
	// Avoid exposing mutable atomics; keep a narrow API for diagnostics.

	namespace detail
	{
		inline std::atomic_uint64_t& Direct()
		{
			static std::atomic_uint64_t v{0};
			return v;
		}
		inline std::atomic_uint64_t& Copy()
		{
			static std::atomic_uint64_t v{0};
			return v;
		}
		inline std::atomic_uint64_t& Compute()
		{
			static std::atomic_uint64_t v{0};
			return v;
		}
	}

	struct Snapshot
	{
		uint64_t Direct = 0;
		uint64_t Copy = 0;
		uint64_t Compute = 0;
	};

	inline void OnSubmit(ED3D12CommandQueueType QueueType)
	{
		if (!D3D12RHI_ShouldEnableMemMon())
			return;
		switch (QueueType)
		{
		case ED3D12CommandQueueType::Default: detail::Direct().fetch_add(1, std::memory_order_relaxed); break;
		case ED3D12CommandQueueType::Copy:    detail::Copy().fetch_add(1, std::memory_order_relaxed); break;
		case ED3D12CommandQueueType::Async:   detail::Compute().fetch_add(1, std::memory_order_relaxed); break;
		default: break;
		}
	}

	inline Snapshot GetSnapshot()
	{
		Snapshot s;
		s.Direct = detail::Direct().load(std::memory_order_relaxed);
		s.Copy = detail::Copy().load(std::memory_order_relaxed);
		s.Compute = detail::Compute().load(std::memory_order_relaxed);
		return s;
	}
}

