#pragma once
#include "RHIPrivate/D3D12RHIPrivate.h"
#include "win/com_ptr.h"

namespace RenderCore
{
	class D3D12CommandAllocator 
	{
	public:
		explicit D3D12CommandAllocator(ID3D12Device* InDevice, const D3D12_COMMAND_LIST_TYPE& InType);
		~D3D12CommandAllocator();

		// The command allocator is ready to be reset when all command lists have been executed (or discarded) AND the GPU not using it.
		inline bool IsReady() const { return (PendingCommandListCount.load() == 0) && SyncPoint.IsComplete(); }
		inline bool HasValidSyncPoint() const { return SyncPoint.IsValid(); }
		inline void SetSyncPoint(const D3D12SyncPoint& InSyncPoint) { assert(InSyncPoint.IsValid()); SyncPoint = InSyncPoint; }
		inline void Reset() { assert(IsReady()); CommandAllocator->Reset(); }

		operator ID3D12CommandAllocator* () { return CommandAllocator.get(); }

		// Called to indicate a command list is using this command alloctor
		inline void IncrementPendingCommandLists()
		{
			assert(PendingCommandListCount.load() >= 0);
			++PendingCommandListCount;
		}

		// Called to indicate a command list using this allocator has been executed OR discarded (closed with no intention to execute it).
		inline void DecrementPendingCommandLists()
		{
			assert(PendingCommandListCount.load() > 0);
			--PendingCommandListCount;
		}

	private:
		void Init(ID3D12Device* InDevice, const D3D12_COMMAND_LIST_TYPE& InType);

	private:
		win32::com_ptr<ID3D12CommandAllocator> CommandAllocator;
		D3D12SyncPoint SyncPoint;	// Indicates when the GPU is finished using the command allocator.
		std::atomic_int32_t PendingCommandListCount;	// The number of command lists using this allocator but haven't been executed yet.
	};
}