#include "D3D12/D3D12DirectCommandListManager.h"
#include "D3D12/D3D12Adapter.h"
#include "D3D12/D3D12CommandList.h"
#include "D3D12/D3D12WindowDevice.h"
#include "D3D12/D3D12RHI.h"

namespace RenderCore
{
	void FD3D12CommandListPayload::Reset()
	{
		NumCommandLists = 0;
		win32::Memzero(CommandLists);

	}

	void FD3D12CommandListPayload::Append(ID3D12CommandList* CL)
	{
		assert(NumCommandLists < FD3D12CommandListPayload::MaxCommandListsPerPayload);

		CommandLists[NumCommandLists] = CL;
		NumCommandLists++;
	}

	FD3D12FenceCore::FD3D12FenceCore(std::weak_ptr<FD3D12Adapter> Parent, uint64_t InitialValue, uint32_t InGPUIndex)
		:FD3D12AdapterChild(Parent)
		,GPUIndex(InGPUIndex)
	{
		assert(!Parent.expired());
		hFenceCompleteEvent = CreateEvent(nullptr, false, false, nullptr);
		assert(INVALID_HANDLE_VALUE != hFenceCompleteEvent);

		HRESULT hr = Parent.lock()->GetD3DDevice()->CreateFence(InitialValue, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(Fence.get_init_ref()));
		assert(SUCCEEDED(hr));
	}

	FD3D12FenceCore::~FD3D12FenceCore()
	{
		if (hFenceCompleteEvent != INVALID_HANDLE_VALUE)
		{
			CloseHandle(hFenceCompleteEvent);
			hFenceCompleteEvent = INVALID_HANDLE_VALUE;
		}
	}

	FD3D12FenceCore* FD3D12FenceCorePool::ObtainFenceCore(uint32_t GPUIndex)
	{
		{
			std::lock_guard<std::recursive_mutex> lock(CS);
			FD3D12FenceCore* Fence = nullptr;
			if (!AvailableFences[GPUIndex].empty() && Fence->IsAvailable())
			{
				Fence = AvailableFences[GPUIndex].front();
				AvailableFences[GPUIndex].pop();
				return Fence;
			}
		}

		return new FD3D12FenceCore(GetParentAdapter(), 0, GPUIndex);
	}

	void FD3D12FenceCorePool::ReleaseFenceCore(FD3D12FenceCore* Fence, uint64_t CurrentFenceValue)
	{
		std::lock_guard<std::recursive_mutex> lock(CS);
		Fence->FenceValueAvailableAt = CurrentFenceValue;
		int32_t index = Fence->GetGPUIndex();
		AvailableFences[index].push(Fence);
	}

	void FD3D12FenceCorePool::Destroy()
	{
		for (uint32_t GPUIndex = 0; GPUIndex < MAX_NUM_GPUS; ++GPUIndex)
		{
			FD3D12FenceCore* Fence = nullptr;
			while (!AvailableFences[GPUIndex].empty())
			{
				Fence = AvailableFences[GPUIndex].front();
				delete Fence;
				AvailableFences[GPUIndex].pop();
			}
		}
	}

	FD3D12Fence::FD3D12Fence(std::weak_ptr<FD3D12Adapter> InParent, const std::wstring& InName /*= L"<unnamed>"*/)
		:FD3D12AdapterChild(InParent)
		, Name(InName)
		, CurrentFence(0)
		, LastSignaledFence(0)
		, LastCompletedFence(0)
	{
		ZeroMemory(FenceCores, sizeof(FenceCores));
		ZeroMemory(LastCompletedFences, sizeof(LastCompletedFences));
	}

	FD3D12Fence::~FD3D12Fence()
	{

	}

	void FD3D12Fence::CreateFence()
	{
		// Can't set the last signaled fence per GPU before a common signal is sent.
		LastSignaledFence = 0;
		const uint32_t GPUIndex = 0;
		Assert(!FenceCores[GPUIndex]);

		// Get a fence from the pool
		FD3D12FenceCore* FenceCore = GetParentAdapter()->GetFenceCorePool().ObtainFenceCore(GPUIndex);
		Assert(FenceCore);
		FenceCores[GPUIndex] = FenceCore;

		LastCompletedFences[GPUIndex] = FenceCore->FenceValueAvailableAt;

		LastCompletedFence = LastCompletedFences[GPUIndex];
		CurrentFence = LastCompletedFences[GPUIndex] + 1;
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

	void FD3D12Fence::GpuWait(uint32_t DeviceGPUIndex, ED3D12CommandQueueType InQueueType, uint64_t FenceValue, uint32_t FenceGPUIndex)
	{
		ID3D12CommandQueue* CommandQueue = GetParentAdapter()->GetDevice(DeviceGPUIndex)->GetD3DCommandQueue(InQueueType);
		Assert(CommandQueue);
		FD3D12FenceCore* FenceCore = FenceCores[FenceGPUIndex];
		Assert(FenceCore);

		VERIFYD3DRESULT(CommandQueue->Wait(FenceCore->GetFence(), FenceValue));
	}

	void FD3D12Fence::GpuWait(ED3D12CommandQueueType InQueueType, uint64_t FenceValue)
	{
		constexpr uint32_t GPUIndex = 0;
		GpuWait(GPUIndex, InQueueType, FenceValue, GPUIndex);
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
			constexpr uint32_t GPUIndex = 0;
			FD3D12FenceCore* FenceCore = FenceCores[GPUIndex];
			Assert(FenceCore);

			if (FenceValue > FenceCore->GetFence()->GetCompletedValue())
			{
				//SCOPE_CYCLE_COUNTER(STAT_D3D12WaitForFenceTime);
				// Multiple threads can be using the same FD3D12Fence (texture streaming).
				std::lock_guard<std::recursive_mutex> Lock(WaitForFenceCS);

				// We must wait.  Do so with an event handler so we don't oversleep.
				VERIFYD3DRESULT(FenceCore->GetFence()->SetEventOnCompletion(FenceValue, FenceCore->GetCompletionEvent()));

				// Wait for the event to complete (the event is automatically reset afterwards)
				const uint32_t WaitResult = WaitForSingleObject(FenceCore->GetCompletionEvent(), INFINITE);
				assert(0 == WaitResult);
			}

			// Refresh the completed fence value
			UpdateLastCompletedFence();
		}
	}

	uint64_t FD3D12Fence::PeekLastCompletedFence() const
	{
		uint64_t CompletedFence = MAXUINT64;
		constexpr uint32_t GPUIndex = 0;
		CompletedFence = std::min<uint64_t>(FenceCores[GPUIndex]->GetFence()->GetCompletedValue(), CompletedFence);
		return CompletedFence;
	}

	uint64_t FD3D12Fence::UpdateLastCompletedFence()
	{
		uint64_t CompletedFence = MAXUINT64;
		constexpr uint32_t GPUIndex = 0;
		FD3D12FenceCore* FenceCore = FenceCores[GPUIndex];
		Assert(FenceCore);
		LastCompletedFences[GPUIndex] = FenceCore->GetFence()->GetCompletedValue();
		CompletedFence = std::min<uint64_t>(LastCompletedFences[GPUIndex], CompletedFence);
		// Must be computed on the stack because the function can be called concurrently.
		LastCompletedFence = CompletedFence;
		return CompletedFence;
	}

	void FD3D12Fence::Destroy()
	{
		constexpr int32_t GPUIndex = 0;
		if (FenceCores[GPUIndex])
		{
			// Return the underlying fence to the pool, store the last value signaled on this fence. 
			// If not fence was signaled since CreateFence() was called, then the last completed value is the last signaled value for this GPU.
			GetParentAdapter()->GetFenceCorePool().ReleaseFenceCore(FenceCores[GPUIndex], LastSignaledFence > 0 ? LastSignaledFence : LastCompletedFences[GPUIndex]);

			FenceCores[GPUIndex] = nullptr;
		}
	}

	void FD3D12Fence::InternalSignal(ED3D12CommandQueueType InQueueType, uint64_t FenceToSignal)
	{
		const uint32_t GPUIndex = 0;
		ID3D12CommandQueue* CommandQueue = GetParentAdapter()->GetDevice(GPUIndex)->GetD3DCommandQueue(InQueueType);
		Assert(CommandQueue);
		FD3D12FenceCore* FenceCore = FenceCores[GPUIndex];
		Assert(FenceCore);

		HRESULT hr = CommandQueue->Signal(FenceCore->GetFence(), FenceToSignal);
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

		// See if the first command allocator in the queue is ready to be reset (will check associated fence)
		D3D12CommandAllocator* pCommandAllocator = nullptr;
		bool isNeedCreated = true;
		if (!CommandAllocatorQueue.empty())
		{
			pCommandAllocator = CommandAllocatorQueue.front();
			if (pCommandAllocator->IsReady())
			{
				isNeedCreated = false;
				CommandAllocatorQueue.pop();
				// Reset the allocator and remove it from the queue.
				pCommandAllocator->Reset();
			}
		}

		if (isNeedCreated)
		{
			// The queue was empty, or no command allocators were ready, so create a new command allocator.
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

		assert(D3DCommandQueue.get() == nullptr);
		assert(ReadyLists.IsEmpty());
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

		D3D12CommandListHandle hList;
		while (!ReadyLists.IsEmpty())
		{
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
		}

		assert(List.GetCommandListType() == CommandListType);
		List.Reset(CommandAllocator);
		return List;
	}

	void FD3D12CommandListManager::ReleaseCommandList(D3D12CommandListHandle& hList)
	{
		assert(hList.IsClosed());
		assert(hList.GetCommandListType() == CommandListType);

		// Indicate that a command list using this allocator has either been executed or discarded.
		hList.CurrentCommandAllocator()->DecrementPendingCommandLists();

		ReadyLists.Enqueue(hList);
	}

	void FD3D12CommandListManager::ExecuteCommandList(D3D12CommandListHandle& hList, bool WaitForCompletion /*= false*/)
	{
		std::vector<D3D12CommandListHandle> Lists;
		Lists.push_back(hList);

		ExecuteCommandLists(Lists, WaitForCompletion);
	}

	void FD3D12CommandListManager::ExecuteCommandLists(std::vector<D3D12CommandListHandle>& Lists, bool WaitForCompletion /*= false*/)
	{
		assert(CommandListFence);

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

		assert(Lists.size() <= FD3D12CommandListPayload::MaxCommandListsPerPayload);
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
						BarrierFenceValue = DirectCommandListManager.ExecuteAndIncrementFence(ComputeBarrierPayload, DirectFence);
						DirectFence.GpuWait(QueueType, BarrierFenceValue);
					}
					else
					{
						CurrentCommandListPayload.Append(barrierCommandList.CommandList());
					}
				}

				CurrentCommandListPayload.Append(commandList.CommandList());
			}
			SignaledFenceValue = ExecuteAndIncrementFence(CurrentCommandListPayload, *CommandListFence);
			SyncPoint = D3D12SyncPoint(CommandListFence.get(), SignaledFenceValue);
			if (CommandListType == D3D12_COMMAND_LIST_TYPE_COMPUTE)
			{
				BarrierSyncPoint = D3D12SyncPoint(&DirectFence, BarrierFenceValue);
			}
			else
			{
				BarrierSyncPoint = SyncPoint;
			}
		}
		else
		{
			for (int32_t i = 0; i < Lists.size(); i++)
			{
				CurrentCommandListPayload.Append(Lists[i].CommandList());
			}
			SignaledFenceValue = ExecuteAndIncrementFence(CurrentCommandListPayload, *CommandListFence);
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

		if (WaitForCompletion)
		{
			CommandListFence->WaitForFence(SignaledFenceValue);
			assert(SyncPoint.IsComplete());
		}
	}

	uint32_t FD3D12CommandListManager::GetResourceBarrierCommandList(D3D12CommandListHandle& hList, D3D12CommandListHandle& hResourceBarrierList)
	{
		std::vector<D3D12PendingResourceBarrier>& PendingResourceBarriers = hList.PendingResourceBarriers();
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
				const D3D12PendingResourceBarrier& PRB = PendingResourceBarriers[i];

				// Should only be doing this for the few resources that need state tracking
				assert(PRB.Resource->RequiresResourceStateTracking());

				CResourceState& ResourceState = PRB.Resource->GetResourceState();

				Desc.Transition.Subresource = PRB.SubResource;
				const D3D12_RESOURCE_STATES Before = ResourceState.GetSubresourceState(Desc.Transition.Subresource);
				const D3D12_RESOURCE_STATES After = PRB.State;

				assert(Before != D3D12_RESOURCE_STATE_TBD && Before != D3D12_RESOURCE_STATE_CORRUPT);
				if (Before != After)
				{
					Desc.Transition.pResource = PRB.Resource->GetResource();
					Desc.Transition.StateBefore = Before;
					Desc.Transition.StateAfter = After;

					// Add the desc
					BarrierDescs.push_back(Desc);
				}

				// Update the state to the what it will be after hList executes
				const D3D12_RESOURCE_STATES CommandListState = hList.GetResourceState(PRB.Resource).GetSubresourceState(Desc.Transition.Subresource);
				const D3D12_RESOURCE_STATES LastState = (CommandListState != D3D12_RESOURCE_STATE_TBD) ? CommandListState : After;

				if (Before != LastState)
				{
					ResourceState.SetSubresourceState(Desc.Transition.Subresource, LastState);
				}
			}

			if (BarrierDescs.size() > 0)
			{
				// Get a new resource barrier command allocator if we don't already have one.
				if (ResourceBarrierCommandAllocator == nullptr)
				{
					ResourceBarrierCommandAllocator = ResourceBarrierCommandAllocatorManager.ObtainCommandAllocator();
				}

				hResourceBarrierList = ObtainCommandList(*ResourceBarrierCommandAllocator);
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
			assert(CommandListFence);
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

	uint64_t FD3D12CommandListManager::ExecuteAndIncrementFence(FD3D12CommandListPayload& Payload, FD3D12Fence& Fence)
	{
		std::lock_guard<std::recursive_mutex> Lock(FenceCS);
		D3DCommandQueue->ExecuteCommandLists(Payload.NumCommandLists, Payload.CommandLists);
		return Fence.Signal(QueueType);
	}

	D3D12CommandListHandle FD3D12CommandListManager::CreateCommandListHandle(D3D12CommandAllocator& CommandAllocator)
	{
		D3D12CommandListHandle List;
		List.Create(GetParentDevice(), CommandListType, CommandAllocator, this);
		return List;
	}

}