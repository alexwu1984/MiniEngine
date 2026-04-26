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

	// Dynamic (shader-visible) descriptor heap churn / traffic.
	inline std::atomic_uint64_t& DynDesc_CreateCount_CbvSrvUav()
	{
		static std::atomic_uint64_t v{0};
		return v;
	}
	inline std::atomic_uint64_t& DynDesc_CreateCount_Sampler()
	{
		static std::atomic_uint64_t v{0};
		return v;
	}
	inline std::atomic_uint64_t& DynDesc_RecycleReadyCount_CbvSrvUav()
	{
		static std::atomic_uint64_t v{0};
		return v;
	}
	inline std::atomic_uint64_t& DynDesc_RecycleReadyCount_Sampler()
	{
		static std::atomic_uint64_t v{0};
		return v;
	}
	inline std::atomic_uint64_t& DynDesc_FenceWaitReuseCount_CbvSrvUav()
	{
		static std::atomic_uint64_t v{0};
		return v;
	}
	inline std::atomic_uint64_t& DynDesc_FenceWaitReuseCount_Sampler()
	{
		static std::atomic_uint64_t v{0};
		return v;
	}
	inline std::atomic_uint64_t& DynDesc_CopyDescriptorsCalls_CbvSrvUav()
	{
		static std::atomic_uint64_t v{0};
		return v;
	}
	inline std::atomic_uint64_t& DynDesc_CopyDescriptorsCalls_Sampler()
	{
		static std::atomic_uint64_t v{0};
		return v;
	}
	inline std::atomic_uint64_t& DynDesc_CopyDescriptorsCount_CbvSrvUav()
	{
		static std::atomic_uint64_t v{0};
		return v;
	}
	inline std::atomic_uint64_t& DynDesc_CopyDescriptorsCount_Sampler()
	{
		static std::atomic_uint64_t v{0};
		return v;
	}

	// Command list pool churn (per FD3D12CommandListManager).
	inline std::atomic_uint64_t& CmdList_ObtainFromReadyCount_Direct()
	{
		static std::atomic_uint64_t v{0};
		return v;
	}
	inline std::atomic_uint64_t& CmdList_CreateCount_Direct()
	{
		static std::atomic_uint64_t v{0};
		return v;
	}
	inline std::atomic_uint64_t& CmdList_ObtainFromReadyCount_Compute()
	{
		static std::atomic_uint64_t v{0};
		return v;
	}
	inline std::atomic_uint64_t& CmdList_CreateCount_Compute()
	{
		static std::atomic_uint64_t v{0};
		return v;
	}

	// Command list submits: distinguish user CLs vs auto-generated barrier CLs.
	inline std::atomic_uint64_t& Submit_UserCLCount_Direct()
	{
		static std::atomic_uint64_t v{0};
		return v;
	}
	inline std::atomic_uint64_t& Submit_BarrierCLCount_Direct()
	{
		static std::atomic_uint64_t v{0};
		return v;
	}
	inline std::atomic_uint64_t& Submit_TotalCLCount_Direct()
	{
		static std::atomic_uint64_t v{0};
		return v;
	}
	inline std::atomic_uint64_t& Submit_ExecCalls_Direct()
	{
		static std::atomic_uint64_t v{0};
		return v;
	}

	inline std::atomic_uint64_t& Submit_UserCLCount_Compute()
	{
		static std::atomic_uint64_t v{0};
		return v;
	}
	inline std::atomic_uint64_t& Submit_BarrierCLCount_Compute()
	{
		static std::atomic_uint64_t v{0};
		return v;
	}
	inline std::atomic_uint64_t& Submit_TotalCLCount_Compute()
	{
		static std::atomic_uint64_t v{0};
		return v;
	}
	inline std::atomic_uint64_t& Submit_ExecCalls_Compute()
	{
		static std::atomic_uint64_t v{0};
		return v;
	}

	inline std::atomic_uint64_t& Submit_UserCLCount_Copy()
	{
		static std::atomic_uint64_t v{0};
		return v;
	}
	inline std::atomic_uint64_t& Submit_BarrierCLCount_Copy()
	{
		static std::atomic_uint64_t v{0};
		return v;
	}
	inline std::atomic_uint64_t& Submit_TotalCLCount_Copy()
	{
		static std::atomic_uint64_t v{0};
		return v;
	}
	inline std::atomic_uint64_t& Submit_ExecCalls_Copy()
	{
		static std::atomic_uint64_t v{0};
		return v;
	}
}

