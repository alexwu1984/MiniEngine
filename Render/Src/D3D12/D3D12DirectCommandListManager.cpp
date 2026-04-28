#include "D3D12/D3D12DirectCommandListManager.h"
#include "RHI/RHI.h"
#include "D3D12/D3D12Adapter.h"
#include "D3D12/D3D12CommandList.h"
#include "D3D12/D3D12WindowDevice.h"
#include "D3D12/D3D12RHI.h"
#include "D3D12/D3D12SubmitStats.h"
#include "D3D12/D3D12CallStats.h"
#include "D3D12/D3D12CreateStats.h"
#include "core/logger.h"

namespace RenderCore
{
	void FD3D12CommandListPayload::Reset()
	{
		NumCommandLists = 0;
		win32::Memzero(CommandLists);

	}

	void FD3D12CommandListPayload::Append(ID3D12CommandList* CL)
	{
		Assert(NumCommandLists < FD3D12CommandListPayload::MaxCommandListsPerPayload);

		CommandLists[NumCommandLists] = CL;
		NumCommandLists++;
	}

	FD3D12FenceCore::FD3D12FenceCore(std::weak_ptr<FD3D12Adapter> Parent, uint64_t InitialValue)
		:FD3D12AdapterChild(Parent)
	{
		Assert(!Parent.expired());
		// When this fence core is first created (not coming from the pool), FenceValueAvailableAt must be valid.
		// It seeds FD3D12Fence::CreateFence()'s starting values. If left uninitialized, fence values become garbage
		// (often pointer-like), breaking all fence-based recycling logic and causing memory growth.
		FenceValueAvailableAt = InitialValue;
		hFenceCompleteEvent = CreateEvent(nullptr, false, false, nullptr);
		Assert(INVALID_HANDLE_VALUE != hFenceCompleteEvent);

		HRESULT hr = Parent.lock()->GetD3DDevice()->CreateFence(InitialValue, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(Fence.get_init_ref()));
		Assert(SUCCEEDED(hr));
	}

	FD3D12FenceCore::~FD3D12FenceCore()
	{
		if (hFenceCompleteEvent != INVALID_HANDLE_VALUE)
		{
			CloseHandle(hFenceCompleteEvent);
			hFenceCompleteEvent = INVALID_HANDLE_VALUE;
		}
	}

	FD3D12FenceCore* FD3D12FenceCorePool::ObtainFenceCore()
	{
		std::lock_guard<std::recursive_mutex> lock(CS);
		if (!AvailableFences.empty())
		{
			FD3D12FenceCore* Fence = AvailableFences.front();
			if (Fence && Fence->IsAvailable())
			{
				AvailableFences.pop();
				return Fence;
			}
		}

		TotalCreated++;
		return new FD3D12FenceCore(GetParentAdapter(), 0);
	}

	void FD3D12FenceCorePool::ReleaseFenceCore(FD3D12FenceCore* Fence, uint64_t CurrentFenceValue)
	{
		std::lock_guard<std::recursive_mutex> lock(CS);
		Fence->FenceValueAvailableAt = CurrentFenceValue;
		AvailableFences.push(Fence);
	}

	void FD3D12FenceCorePool::Destroy()
	{
		FD3D12FenceCore* Fence = nullptr;
		while (!AvailableFences.empty())
		{
			Fence = AvailableFences.front();
			delete Fence;
			AvailableFences.pop();
		}
	}

	FD3D12Fence::FD3D12Fence(std::weak_ptr<FD3D12Adapter> InParent, const std::wstring& InName /*= L"<unnamed>"*/)
		:FD3D12AdapterChild(InParent)
		, Name(InName)
		, CurrentFence(0)
		, LastSignaledFence(0)
		, LastCompletedFence(0)
	{
	}

	FD3D12Fence::~FD3D12Fence()
	{
		Destroy();
	}

	void FD3D12Fence::CreateFence()
	{
		// Can't set the last signaled fence per GPU before a common signal is sent.
		LastSignaledFence = 0;
		const uint32_t GPUIndex = 0;
		Assert(!FenceCoreCache);

		// Get a fence from the pool
		FD3D12FenceCore* FenceCore = GetParentAdapter()->GetFenceCorePool().ObtainFenceCore();
		Assert(FenceCore);
		FenceCoreCache = FenceCore;

		LastCompletedFenceCache = FenceCore->FenceValueAvailableAt;

		LastCompletedFence = LastCompletedFenceCache;
		CurrentFence = LastCompletedFenceCache + 1;
	}

	uint64_t FD3D12Fence::Signal(ED3D12CommandQueueType InQueueType)
	{
		Assert(LastSignaledFence != CurrentFence);
		InternalSignal(InQueueType, CurrentFence);

		// Update the cached version of the fence value
		UpdateLastCompletedFence();

		// Increment the current Fence
		CurrentFence++;

		return LastSignaledFence;
	}

	void FD3D12Fence::GpuWait(ED3D12CommandQueueType InQueueType, uint64_t FenceValue)
	{
		ID3D12CommandQueue* CommandQueue = GetParentAdapter()->GetDevice()->GetD3DCommandQueue(InQueueType);
		Assert(CommandQueue);
		Assert(FenceCoreCache);

		VERIFYD3DRESULT(CommandQueue->Wait(FenceCoreCache->GetFence(), FenceValue));
	}

	bool FD3D12Fence::IsFenceComplete(uint64_t FenceValue)
	{
		Assert(FenceValue <= CurrentFence);

		// Avoid repeatedly calling GetCompletedValue()
		if (FenceValue <= LastCompletedFence)
		{
			return true;
		}

		// Refresh the completed fence value
		return FenceValue <= UpdateLastCompletedFence();
	}

	void FD3D12Fence::WaitForFence(uint64_t FenceValue)
	{
		if (!IsFenceComplete(FenceValue))
		{
			Assert(FenceCoreCache);

			if (FenceValue > FenceCoreCache->GetFence()->GetCompletedValue())
			{
				//SCOPE_CYCLE_COUNTER(STAT_D3D12WaitForFenceTime);
				// Multiple threads can be using the same FD3D12Fence (texture streaming).
				std::lock_guard<std::recursive_mutex> Lock(WaitForFenceCS);

				// We must wait.  Do so with an event handler so we don't oversleep.
				Render::D3D12CallStats::IncFenceSetEventOnCompletion();
				VERIFYD3DRESULT(FenceCoreCache->GetFence()->SetEventOnCompletion(FenceValue, FenceCoreCache->GetCompletionEvent()));

				// Wait for the event to complete (the event is automatically reset afterwards)
				Render::D3D12CallStats::IncWaitForSingleObject();
				const uint32_t WaitResult = WaitForSingleObject(FenceCoreCache->GetCompletionEvent(), INFINITE);
				Assert(0 == WaitResult);
			}

			// Refresh the completed fence value
			UpdateLastCompletedFence();
		}
	}

	uint64_t FD3D12Fence::PeekLastCompletedFence() const
	{
		Assert(FenceCoreCache);
		uint64_t CompletedFence = MAXUINT64;
		CompletedFence = std::min<uint64_t>(FenceCoreCache->GetFence()->GetCompletedValue(), CompletedFence);
		return CompletedFence;
	}

	uint64_t FD3D12Fence::UpdateLastCompletedFence()
	{
		uint64_t CompletedFence = MAXUINT64;
		Assert(FenceCoreCache);
		LastCompletedFenceCache = FenceCoreCache->GetFence()->GetCompletedValue();
		CompletedFence = std::min<uint64_t>(LastCompletedFenceCache, CompletedFence);
		// Must be computed on the stack because the function can be called concurrently.
		LastCompletedFence = CompletedFence;
		return CompletedFence;
	}

	void FD3D12Fence::Destroy()
	{
		if (!FenceCoreCache)
			return;
		// Return the core to the pool when the adapter still exists; otherwise the pool may already be gone (shutdown order).
		if (auto Adapter = TryGetParentAdapter())
		{
			Adapter->GetFenceCorePool().ReleaseFenceCore(FenceCoreCache, LastSignaledFence > 0 ? LastSignaledFence : LastCompletedFenceCache);
		}
		else
		{
			delete FenceCoreCache;
		}
		FenceCoreCache = nullptr;
	}

	void FD3D12Fence::InternalSignal(ED3D12CommandQueueType InQueueType, uint64_t FenceToSignal)
	{
		ID3D12CommandQueue* CommandQueue = GetParentAdapter()->GetDevice()->GetD3DCommandQueue(InQueueType);
		Assert(CommandQueue);
		Assert(FenceCoreCache);

		Render::D3D12CallStats::IncQueueSignal();
		// (diagnostic logging removed)
		HRESULT hr = CommandQueue->Signal(FenceCoreCache->GetFence(), FenceToSignal);
		Assert(SUCCEEDED(hr));
		LastSignaledFence = FenceToSignal;
	}

	uint64_t FD3D12ManualFence::Signal(ED3D12CommandQueueType InQueueType, uint64_t FenceToSignal)
	{
		Assert(LastSignaledFence != FenceToSignal);
		InternalSignal(InQueueType, FenceToSignal);

		// Update the cached version of the fence value
		UpdateLastCompletedFence();
		Assert(LastSignaledFence == FenceToSignal);

		return LastSignaledFence;
	}

	FD3D12CommandAllocatorManager::FD3D12CommandAllocatorManager(std::weak_ptr<FD3D12Device> InParent, const D3D12_COMMAND_LIST_TYPE& InType)
		:FD3D12DeviceChild(InParent)
		, Type(InType)
	{

	}

	FD3D12CommandAllocatorManager::~FD3D12CommandAllocatorManager()
	{
		// Go through all command allocators owned by this manager and delete them.
		for (auto Iter = CommandAllocators.begin(); Iter != CommandAllocators.end(); ++Iter)
		{
			D3D12CommandAllocator* pCommandAllocator = *Iter;
			delete pCommandAllocator;
		}
	}

	D3D12CommandAllocator* FD3D12CommandAllocatorManager::ObtainCommandAllocator()
	{
		std::lock_guard<std::recursive_mutex> Lock(CS);

		// Match UE 4.26 FD3D12CommandAllocatorManager::ObtainCommandAllocator (D3D12DirectCommandListManager.cpp):
		// only the head of the queue may be taken if it is already GPU-ready; otherwise allocate a new allocator.
		D3D12CommandAllocator* pCommandAllocator = nullptr;
		bool isNeedCreated = true;
		if (!CommandAllocatorQueue.empty())
		{
			pCommandAllocator = CommandAllocatorQueue.front();
			if (pCommandAllocator->IsReady())
			{
				CommandAllocatorQueue.pop();
				pCommandAllocator->Reset();
				isNeedCreated = false;
			}
			else
			{
				pCommandAllocator = nullptr;
			}
		}

		if (isNeedCreated)
		{
			// The queue was empty, or the allocator at the head was not ready, so create a new command allocator.
			pCommandAllocator = new D3D12CommandAllocator(GetParentDevice()->GetDevice(), Type);
			Assert(pCommandAllocator);
			CommandAllocators.push_back(pCommandAllocator);	// The command allocator's lifetime is managed by this manager

			// Set a valid sync point
			FD3D12Fence& FrameFence = GetParentDevice()->GetParentAdapter()->GetFrameFence();
			const D3D12SyncPoint SyncPoint(&FrameFence, FrameFence.UpdateLastCompletedFence());
			pCommandAllocator->SetSyncPoint(SyncPoint);
		}

		Assert(pCommandAllocator->IsReady());
		return pCommandAllocator;
	}

	void FD3D12CommandAllocatorManager::ReleaseCommandAllocator(D3D12CommandAllocator* CommandAllocator)
	{
		std::lock_guard<std::recursive_mutex> Lock(CS);
		Assert(CommandAllocator->HasValidSyncPoint());
		CommandAllocatorQueue.push(CommandAllocator);
	}

	FD3D12CommandListManager::FD3D12CommandListManager(std::weak_ptr<FD3D12Device> InParent, D3D12_COMMAND_LIST_TYPE InCommandListType, ED3D12CommandQueueType InQueueType)
		:FD3D12DeviceChild(InParent)
		, ResourceBarrierCommandAllocatorManager(InParent,InCommandListType)
		, CommandListType(InCommandListType)
		, QueueType(InQueueType)
	{

	}

	FD3D12CommandListManager::~FD3D12CommandListManager()
	{
		Destroy();
	}

	void FD3D12CommandListManager::Create(const wchar_t* Name, uint32_t NumCommandLists /*= 0*/, uint32_t Priority /*= 0*/)
	{
		auto Device = GetParentDevice();
		std::shared_ptr<FD3D12Adapter> Adapter = Device->GetParentAdapter();

		CommandListFence = std::make_shared<FD3D12Fence>(Adapter, L"Command List Fence");
		CommandListFence->CreateFence();

		Assert(D3DCommandQueue.get() == nullptr);
		Assert(ReadyLists.IsEmpty());
		//checkf(NumCommandLists <= 0xffff, TEXT("Exceeded maximum supported command lists"));

		D3D12_COMMAND_QUEUE_DESC CommandQueueDesc = {};
		CommandQueueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
		CommandQueueDesc.NodeMask = 1;
		CommandQueueDesc.Priority = Priority;
		CommandQueueDesc.Type = CommandListType;
		D3DCommandQueue = Adapter->GetOwningRHI()->CreateCommandQueue(Device, CommandQueueDesc);

		if (NumCommandLists > 0)
		{
			// Create a temp command allocator for command list creation.
			D3D12CommandAllocator TempCommandAllocator(Device->GetDevice(), CommandListType);
			for (uint32_t i = 0; i < NumCommandLists; ++i)
			{
				D3D12CommandListHandle hList = CreateCommandListHandle(TempCommandAllocator);
				ReadyLists.Enqueue(hList);
			}
		}
	}

	void FD3D12CommandListManager::Destroy()
	{
		// Wait for the queue to empty
		WaitForCommandQueueFlush();

		D3DCommandQueue.reset();

		while (!ReadyLists.IsEmpty())
		{
			D3D12CommandListHandle hList;
			ReadyLists.Dequeue(hList);
		}

		if (CommandListFence)
		{
			CommandListFence->Destroy();
			CommandListFence.reset();
		}
	}

	D3D12CommandListHandle FD3D12CommandListManager::ObtainCommandList(D3D12CommandAllocator& CommandAllocator)
	{
		D3D12CommandListHandle List;
		if (!ReadyLists.Dequeue(List))
		{
			// Create a command list if there are none available.
			List = CreateCommandListHandle(CommandAllocator);
			if (CommandListType == D3D12_COMMAND_LIST_TYPE_DIRECT)
				D3D12MemMonAtomicAdd(D3D12CreateStats::CmdList_CreateCount_Direct());
			else if (CommandListType == D3D12_COMMAND_LIST_TYPE_COMPUTE)
				D3D12MemMonAtomicAdd(D3D12CreateStats::CmdList_CreateCount_Compute());
		}
		else
		{
			if (CommandListType == D3D12_COMMAND_LIST_TYPE_DIRECT)
				D3D12MemMonAtomicAdd(D3D12CreateStats::CmdList_ObtainFromReadyCount_Direct());
			else if (CommandListType == D3D12_COMMAND_LIST_TYPE_COMPUTE)
				D3D12MemMonAtomicAdd(D3D12CreateStats::CmdList_ObtainFromReadyCount_Compute());
		}

		Assert(List.GetCommandListType() == CommandListType);
		List.Reset(CommandAllocator);
		return List;
	}

	void FD3D12CommandListManager::ReleaseCommandList(D3D12CommandListHandle& hList)
	{
		Assert(hList.IsClosed());
		Assert(hList.GetCommandListType() == CommandListType);

		// Indicate that a command list using this allocator has either been executed or discarded.
		hList.CurrentCommandAllocator()->DecrementPendingCommandLists();

		ReadyLists.Enqueue(hList);
	}

	uint64_t FD3D12CommandListManager::ExecuteCommandList(D3D12CommandListHandle& hList, 
														 const std::function<void(uint64_t FenceID)>& OnClearResource, bool WaitForCompletion /*= false*/)
	{
		std::vector<D3D12CommandListHandle> Lists;
		Lists.push_back(hList);

		return ExecuteCommandLists(Lists, OnClearResource, WaitForCompletion);
	}

	uint64_t FD3D12CommandListManager::ExecuteCommandLists(std::vector<D3D12CommandListHandle>& Lists, 
		                                                  const std::function<void(uint64_t FenceID)>& OnClearResource, bool WaitForCompletion /*= false*/)
	{
		Assert(CommandListFence.get());

		bool NeedsResourceBarriers = false;
		for (int32_t i = 0; i < Lists.size(); i++)
		{
			D3D12CommandListHandle& commandList = Lists[i];
			if (commandList.PendingResourceBarriers().size() > 0)
			{
				NeedsResourceBarriers = true;
				break;
			}
		}

		uint64_t SignaledFenceValue = -1;
		uint64_t BarrierFenceValue = -1;
		D3D12SyncPoint SyncPoint;
		D3D12SyncPoint BarrierSyncPoint;

		FD3D12CommandListManager& DirectCommandListManager = GetParentDevice()->GetCommandListManager();
		FD3D12Fence& DirectFence = DirectCommandListManager.GetFence();
		//checkf(DirectFence.GetGPUMask() == GetGPUMask(), TEXT("Fence GPU masks does not fit with the command list mask!"));

		int32_t commandListIndex = 0;
		int32_t barrierCommandListIndex = 0;

		// Close the resource barrier lists, get the raw command list pointers, and enqueue the command list handles
		// Note: All command lists will share the same fence
		FD3D12CommandListPayload CurrentCommandListPayload;
		FD3D12CommandListPayload ComputeBarrierPayload;

		Assert(Lists.size() <= FD3D12CommandListPayload::MaxCommandListsPerPayload);
		D3D12CommandListHandle BarrierCommandList[128];
		if (NeedsResourceBarriers)
		{
			std::lock_guard<std::recursive_mutex> Lock(ResourceStateCS);

			for (int32_t i = 0; i < Lists.size(); i++)
			{
				D3D12CommandListHandle& commandList = Lists[i];

				D3D12CommandListHandle barrierCommandList = {};
				// Async compute cannot perform all resource transitions, and so it uses the direct context
				const uint32_t numBarriers = DirectCommandListManager.GetResourceBarrierCommandList(commandList, barrierCommandList);
				if (numBarriers)
				{
					// TODO: Unnecessary assignment here, but fixing this will require refactoring GetResourceBarrierCommandList
					BarrierCommandList[barrierCommandListIndex] = barrierCommandList;
					barrierCommandListIndex++;

					barrierCommandList.Close();

					if (CommandListType == D3D12_COMMAND_LIST_TYPE_COMPUTE)
					{
						ComputeBarrierPayload.Reset();
						ComputeBarrierPayload.Append(barrierCommandList.CommandList());
						BarrierFenceValue = DirectCommandListManager.ExecuteAndIncrementFence(ComputeBarrierPayload, DirectFence, true /*bForceSignal*/);
						DirectFence.GpuWait(QueueType, BarrierFenceValue);
						// This path bypasses ExecuteAndClear; still must recycle upload pages / dynamic heaps for the barrier list.
						BarrierCommandList[barrierCommandListIndex - 1].CleanupTransientResources(BarrierFenceValue, ED3D12CommandQueueType::Default);
					}
					else
					{
						CurrentCommandListPayload.Append(barrierCommandList.CommandList());
					}
				}

				CurrentCommandListPayload.Append(commandList.CommandList());
			}
			{
				const uint64_t user = (uint64_t)Lists.size();
				const uint64_t barrier = (uint64_t)barrierCommandListIndex;
				const uint64_t total = (uint64_t)CurrentCommandListPayload.NumCommandLists;
				if (CommandListType == D3D12_COMMAND_LIST_TYPE_DIRECT)
				{
					D3D12MemMonAtomicAdd(D3D12CreateStats::Submit_UserCLCount_Direct(), user);
					D3D12MemMonAtomicAdd(D3D12CreateStats::Submit_BarrierCLCount_Direct(), barrier);
					D3D12MemMonAtomicAdd(D3D12CreateStats::Submit_TotalCLCount_Direct(), total);
				}
				else if (CommandListType == D3D12_COMMAND_LIST_TYPE_COMPUTE)
				{
					D3D12MemMonAtomicAdd(D3D12CreateStats::Submit_UserCLCount_Compute(), user);
					D3D12MemMonAtomicAdd(D3D12CreateStats::Submit_BarrierCLCount_Compute(), barrier);
					D3D12MemMonAtomicAdd(D3D12CreateStats::Submit_TotalCLCount_Compute(), total);
				}
				else if (CommandListType == D3D12_COMMAND_LIST_TYPE_COPY)
				{
					D3D12MemMonAtomicAdd(D3D12CreateStats::Submit_UserCLCount_Copy(), user);
					D3D12MemMonAtomicAdd(D3D12CreateStats::Submit_BarrierCLCount_Copy(), barrier);
					D3D12MemMonAtomicAdd(D3D12CreateStats::Submit_TotalCLCount_Copy(), total);
				}
			}
			SignaledFenceValue = ExecuteAndIncrementFence(CurrentCommandListPayload, *CommandListFence, WaitForCompletion);
			for (int32_t i = 0; i < Lists.size(); i++)
				Lists[i].CommitTrackedResourceStateToGlobal();
			SyncPoint = D3D12SyncPoint(CommandListFence.get(), SignaledFenceValue);
			if (CommandListType == D3D12_COMMAND_LIST_TYPE_COMPUTE)
			{
				BarrierSyncPoint = D3D12SyncPoint(&DirectFence, BarrierFenceValue);
			}
			else
			{
				BarrierSyncPoint = SyncPoint;
			}

			// Direct-queue submits bundle user CLs with auto-generated barrier CLs in one Execute; only the outer ExecuteAndClear hook runs for the user list.
			if (CommandListType != D3D12_COMMAND_LIST_TYPE_COMPUTE && barrierCommandListIndex > 0)
			{
				for (int32_t bi = 0; bi < barrierCommandListIndex; ++bi)
					BarrierCommandList[bi].CleanupTransientResources(SignaledFenceValue, QueueType);
			}
		}
		else
		{
			for (int32_t i = 0; i < Lists.size(); i++)
			{
				CurrentCommandListPayload.Append(Lists[i].CommandList());
			}
			{
				const uint64_t user = (uint64_t)Lists.size();
				const uint64_t barrier = 0;
				const uint64_t total = (uint64_t)CurrentCommandListPayload.NumCommandLists;
				if (CommandListType == D3D12_COMMAND_LIST_TYPE_DIRECT)
				{
					D3D12MemMonAtomicAdd(D3D12CreateStats::Submit_UserCLCount_Direct(), user);
					D3D12MemMonAtomicAdd(D3D12CreateStats::Submit_BarrierCLCount_Direct(), barrier);
					D3D12MemMonAtomicAdd(D3D12CreateStats::Submit_TotalCLCount_Direct(), total);
				}
				else if (CommandListType == D3D12_COMMAND_LIST_TYPE_COMPUTE)
				{
					D3D12MemMonAtomicAdd(D3D12CreateStats::Submit_UserCLCount_Compute(), user);
					D3D12MemMonAtomicAdd(D3D12CreateStats::Submit_BarrierCLCount_Compute(), barrier);
					D3D12MemMonAtomicAdd(D3D12CreateStats::Submit_TotalCLCount_Compute(), total);
				}
				else if (CommandListType == D3D12_COMMAND_LIST_TYPE_COPY)
				{
					D3D12MemMonAtomicAdd(D3D12CreateStats::Submit_UserCLCount_Copy(), user);
					D3D12MemMonAtomicAdd(D3D12CreateStats::Submit_BarrierCLCount_Copy(), barrier);
					D3D12MemMonAtomicAdd(D3D12CreateStats::Submit_TotalCLCount_Copy(), total);
				}
			}
			SignaledFenceValue = ExecuteAndIncrementFence(CurrentCommandListPayload, *CommandListFence, WaitForCompletion);
			for (int32_t i = 0; i < Lists.size(); i++)
				Lists[i].CommitTrackedResourceStateToGlobal();
			//check(CommandListType != D3D12_COMMAND_LIST_TYPE_COMPUTE);
			SyncPoint = D3D12SyncPoint(CommandListFence.get(), SignaledFenceValue);
			BarrierSyncPoint = SyncPoint;
		}

		for (int32_t i = 0; i < Lists.size(); i++)
		{
			D3D12CommandListHandle& commandList = Lists[i];

			// Set a sync point on the command list so we know when it's current generation is complete on the GPU, then release it so it can be reused later.
			// Note this also updates the command list's command allocator
			commandList.SetSyncPoint(SyncPoint);
			ReleaseCommandList(commandList);
		}

		for (int32_t i = 0; i < barrierCommandListIndex; i++)
		{
			D3D12CommandListHandle& commandList = BarrierCommandList[i];

			// Set a sync point on the command list so we know when it's current generation is complete on the GPU, then release it so it can be reused later.
			// Note this also updates the command list's command allocator
			commandList.SetSyncPoint(BarrierSyncPoint);
			DirectCommandListManager.ReleaseCommandList(commandList);
		}

		if (OnClearResource)
		{
			OnClearResource(SignaledFenceValue);
		}

		if (WaitForCompletion)
		{
			CommandListFence->WaitForFence(SignaledFenceValue);
			Assert(SyncPoint.IsComplete());
		}
		return SignaledFenceValue;
	}

	uint32_t FD3D12CommandListManager::GetResourceBarrierCommandList(D3D12CommandListHandle& hList, D3D12CommandListHandle& hResourceBarrierList)
	{
		std::vector<FD3D12PendingResourceBarrier>& PendingResourceBarriers = hList.PendingResourceBarriers();
		const uint32_t NumPendingResourceBarriers = (uint32_t)PendingResourceBarriers.size();
		if (NumPendingResourceBarriers)
		{
			// Reserve space for the descs
			std::vector<D3D12_RESOURCE_BARRIER> BarrierDescs;
			BarrierDescs.reserve(NumPendingResourceBarriers);

			// Fill out the descs
			D3D12_RESOURCE_BARRIER Desc = {};
			Desc.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;

			for (uint32_t i = 0; i < NumPendingResourceBarriers; ++i)
			{
				const FD3D12PendingResourceBarrier& PRB = PendingResourceBarriers[i];

				// Should only be doing this for the few resources that need state tracking
				Assert(PRB.Resource->RequiresResourceStateTracking());

				CResourceState& ResourceState = PRB.Resource->GetResourceState();
				CResourceState& ClResourceState = hList.GetResourceState(PRB.Resource);

				Desc.Transition.pResource = PRB.Resource->GetResource();
				const D3D12_RESOURCE_STATES After = PRB.State;

				// Pending entries may use ALL_SUBRESOURCES. Once global tracking has split to per-subresource,
				// CResourceState::GetSubresourceState(ALL) is invalid (would index past the array). Expand to
				// one transition per subresource so StateBefore matches the runtime (fixes D3D12 #523).
				if (PRB.SubResource == D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES)
				{
					const uint32_t NumSub = PRB.Resource->GetSubresourceCount();
					for (uint32_t sub = 0; sub < NumSub; ++sub)
					{
						Desc.Transition.Subresource = sub;
						const D3D12_RESOURCE_STATES Before = ResourceState.GetSubresourceState(sub);
						Assert(Before != D3D12_RESOURCE_STATE_TBD && Before != D3D12_RESOURCE_STATE_CORRUPT);
						if (Before != After)
						{
							Desc.Transition.StateBefore = Before;
							Desc.Transition.StateAfter = After;
							BarrierDescs.push_back(Desc);
						}

						const D3D12_RESOURCE_STATES CommandListState = ClResourceState.GetSubresourceState(sub);
						const D3D12_RESOURCE_STATES LastState = (CommandListState != D3D12_RESOURCE_STATE_TBD) ? CommandListState : After;

						if (Before != LastState)
						{
							ResourceState.SetSubresourceState(sub, LastState);
						}
					}
				}
				else
				{
					Desc.Transition.Subresource = PRB.SubResource;
					const D3D12_RESOURCE_STATES Before = ResourceState.GetSubresourceState(Desc.Transition.Subresource);
					Assert(Before != D3D12_RESOURCE_STATE_TBD && Before != D3D12_RESOURCE_STATE_CORRUPT);
					if (Before != After)
					{
						Desc.Transition.StateBefore = Before;
						Desc.Transition.StateAfter = After;
						BarrierDescs.push_back(Desc);
					}

					const D3D12_RESOURCE_STATES CommandListState = ClResourceState.GetSubresourceState(Desc.Transition.Subresource);
					const D3D12_RESOURCE_STATES LastState = (CommandListState != D3D12_RESOURCE_STATE_TBD) ? CommandListState : After;

					if (Before != LastState)
					{
						ResourceState.SetSubresourceState(Desc.Transition.Subresource, LastState);
					}
				}
			}

			PendingResourceBarriers.clear();

			if (BarrierDescs.size() > 0)
			{
				// Get a new resource barrier command allocator if we don't already have one.
				if (ResourceBarrierCommandAllocator == nullptr)
				{
					ResourceBarrierCommandAllocator = ResourceBarrierCommandAllocatorManager.ObtainCommandAllocator();
				}

				hResourceBarrierList = ObtainCommandList(*ResourceBarrierCommandAllocator);
				// Inherit owning context so post-submit cleanup can retire dynamic heaps/linear pages consistently with the list that produced the barriers.
				hResourceBarrierList.SetCurrentOwningContext(hList.GetCurrentOwningContext());
				hResourceBarrierList->ResourceBarrier((uint32_t)BarrierDescs.size(), BarrierDescs.data());
			}

			return (uint32_t)BarrierDescs.size();
		}

		return 0;
	}

	CommandListState FD3D12CommandListManager::GetCommandListState(const D3D12CLSyncPoint& hSyncPoint)
	{
		assert(hSyncPoint);
		if (hSyncPoint.IsComplete())
		{
			return CommandListState::kFinished;
		}
		else if (hSyncPoint.Generation == hSyncPoint.CommandList.CurrentGeneration())
		{
			return CommandListState::kOpen;
		}
		else
		{
			return CommandListState::kQueued;
		}
	}

	bool FD3D12CommandListManager::IsComplete(const D3D12CLSyncPoint& hSyncPoint, uint64_t FenceOffset /*= 0*/)
	{
		if (!hSyncPoint)
		{
			return false;
		}

		//checkf(FenceOffset == 0, TEXT("This currently doesn't support offsetting fence values."));
		return hSyncPoint.IsComplete();
	}

	void FD3D12CommandListManager::WaitForCommandQueueFlush()
	{
		if (D3DCommandQueue)
		{
			Assert(CommandListFence.get());
			const uint64_t SignaledFence = CommandListFence->Signal(QueueType);
			CommandListFence->WaitForFence(SignaledFence);
		}
	}

	void FD3D12CommandListManager::ReleaseResourceBarrierCommandListAllocator()
	{
		// Release the resource barrier command allocator.
		if (ResourceBarrierCommandAllocator != nullptr)
		{
			ResourceBarrierCommandAllocatorManager.ReleaseCommandAllocator(ResourceBarrierCommandAllocator);
			ResourceBarrierCommandAllocator = nullptr;
		}
	}

	uint64_t FD3D12CommandListManager::ExecuteAndIncrementFence(FD3D12CommandListPayload& Payload, FD3D12Fence& Fence, bool bForceSignal)
	{
		std::lock_guard<std::recursive_mutex> Lock(FenceCS);
		// Shutdown / teardown safety: never call into D3D12Core with invalid command list pointers.
		// This can happen if a context tries to flush after its command list handle has been torn down.
		if (Payload.NumCommandLists == 0)
		{
			D3D12SubmitStats::OnSubmit(QueueType);
			if (QueueType == ED3D12CommandQueueType::Default)
				Render::D3D12CallStats::IncDirectFenceImmediateSignal();
			return Fence.Signal(QueueType);
		}
		for (uint32_t i = 0; i < Payload.NumCommandLists; ++i)
		{
			if (Payload.CommandLists[i] == nullptr)
			{
				core::LOG(core::log_err, L"[D3D12] ExecuteCommandLists aborted: null command list (queue=%d i=%u n=%u)", (int)QueueType, i, Payload.NumCommandLists);
				Payload.NumCommandLists = 0;
				break;
			}
		}
		if (Payload.NumCommandLists == 0)
		{
			D3D12SubmitStats::OnSubmit(QueueType);
			if (QueueType == ED3D12CommandQueueType::Default)
				Render::D3D12CallStats::IncDirectFenceImmediateSignal();
			return Fence.Signal(QueueType);
		}

		if (CommandListType == D3D12_COMMAND_LIST_TYPE_DIRECT)
			D3D12MemMonAtomicAdd(D3D12CreateStats::Submit_ExecCalls_Direct());
		else if (CommandListType == D3D12_COMMAND_LIST_TYPE_COMPUTE)
			D3D12MemMonAtomicAdd(D3D12CreateStats::Submit_ExecCalls_Compute());
		else if (CommandListType == D3D12_COMMAND_LIST_TYPE_COPY)
			D3D12MemMonAtomicAdd(D3D12CreateStats::Submit_ExecCalls_Copy());
		Render::D3D12CallStats::IncExecuteCommandLists((uint32_t)Payload.NumCommandLists);
		D3DCommandQueue->ExecuteCommandLists(Payload.NumCommandLists, Payload.CommandLists);

		// Track submits per queue type (diagnostics).
		D3D12SubmitStats::OnSubmit(QueueType);

		// Always signal after Execute so fence-tied retire/recycle paths progress deterministically.
		(void)bForceSignal;
		if (QueueType == ED3D12CommandQueueType::Default)
			Render::D3D12CallStats::IncDirectFenceImmediateSignal();
		return Fence.Signal(QueueType);
	}

	D3D12CommandListHandle FD3D12CommandListManager::CreateCommandListHandle(D3D12CommandAllocator& CommandAllocator)
	{
		D3D12CommandListHandle List;
		List.Create(GetParentDevice(), CommandListType, CommandAllocator, this);
		return List;
	}

}