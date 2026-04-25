#include "D3D12/D3D12CommandList.h"
#include "D3D12/D3D12DirectCommandListManager.h"
#include "D3D12/D3D12CommandContext.h"
#include "D3D12/D3D12WindowDevice.h"
#include "D3D12/D3D12StateCache.h"

namespace RenderCore
{

	D3D12CommandAllocator::D3D12CommandAllocator(ID3D12Device* InDevice, const D3D12_COMMAND_LIST_TYPE& InType)
	{
		Init(InDevice, InType);
	}

	D3D12CommandAllocator::~D3D12CommandAllocator()
	{
		CommandAllocator.reset();
	}

	void D3D12CommandAllocator::Init(ID3D12Device* InDevice, const D3D12_COMMAND_LIST_TYPE& InType)
	{
		Assert(CommandAllocator.get() == nullptr);
		HRESULT hr = InDevice->CreateCommandAllocator(InType, IID_PPV_ARGS(CommandAllocator.get_init_ref()));
	}

	D3D12CommandListHandle::D3D12CommandListData::D3D12CommandListData(std::weak_ptr<FD3D12Device> ParentDevice, D3D12_COMMAND_LIST_TYPE InCommandListType, 
																	   D3D12CommandAllocator& CommandAllocator, FD3D12CommandListManager* InCommandListManager)
		:FD3D12DeviceChild(ParentDevice)
		, CommandListType(InCommandListType)
		, CurrentCommandAllocator(&CommandAllocator)
		, CommandListManager(InCommandListManager)
		, CurrentGeneration(1)
		, LastCompleteGeneration(0)
		, IsClosed(false)
		, bShouldTrackStartEndTime(false)
		, CpuLinearAllocator(ELinearAllocatorType::CpuWritable, ParentDevice)
		, GpuLinearAllocator(ELinearAllocatorType::GpuExclusive, ParentDevice)

	{
		VERIFYD3DRESULT(GetParentDevice()->GetDevice()->CreateCommandList(0, CommandListType, CommandAllocator, nullptr, IID_PPV_ARGS(CommandList.get_init_ref())));
		CommandList->QueryInterface(IID_PPV_ARGS(CommandList1.get_init_ref()));
		Close();
		PendingResourceBarriers.reserve(256);
	}

	void D3D12CommandListHandle::D3D12CommandListData::Close()
	{
		if (!IsClosed)
		{
			FlushResourceBarriers();
			VERIFYD3DRESULT(CommandList->Close());

			IsClosed = true;
		}
	}

	void D3D12CommandListHandle::D3D12CommandListData::Reset(D3D12CommandAllocator& CommandAllocator)
	{
		if (CommandAllocator.IsReady())
			CommandAllocator.Reset();
		VERIFYD3DRESULT(CommandList->Reset(CommandAllocator, nullptr));

		CurrentCommandAllocator = &CommandAllocator;
		IsClosed = false;

		// Indicate this command allocator is being used.
		CurrentCommandAllocator->IncrementPendingCommandLists();

		CleanupActiveGenerations();

		// Remove all pendering barriers from the command list
		PendingResourceBarriers.clear();

		// Per-list tracked states are only needed until the list is submitted; after the allocator
		// is ready again, global resource state has been updated. Leaving this map populated causes
		// unbounded growth across frames for every distinct tracked resource pointer.
		TrackedResourceState.Empty();

		// If this fails then some previous resource barriers were never submitted.
		Assert(ResourceBarrierBatcher.GetBarriers().size() == 0);

	}

	D3D12CommandListHandle::D3D12CommandListData::~D3D12CommandListData()
	{

	}

	void D3D12CommandListHandle::Create(std::weak_ptr<FD3D12Device> ParentDevice, D3D12_COMMAND_LIST_TYPE CommandListType, D3D12CommandAllocator& CommandAllocator, FD3D12CommandListManager* InCommandListManager)
	{
		Assert(!CommandListData);
		CommandListData = new D3D12CommandListData(ParentDevice, CommandListType, CommandAllocator, InCommandListManager);
		CommandListData->AddRef();
	}

	void D3D12CommandListHandle::ExecuteAndClear(bool WaitForCompletion /*= false*/)
	{
		Assert(CommandListData);
		CommandListData->CommandListManager->ExecuteCommandList(*this, [this](uint64_t FenceID) {
			const ED3D12CommandQueueType QueueType = CommandListData->CommandListManager->GetQueueType();
			CommandListData->CpuLinearAllocator.CleanupUsedPages(FenceID, QueueType);
			CommandListData->GpuLinearAllocator.CleanupUsedPages(FenceID, QueueType);
			GetCurrentOwningContext()->CleanupUsedHeaps(FenceID, QueueType);
			}, WaitForCompletion);
	}

	void D3D12CommandListHandle::Execute(bool WaitForCompletion /*= false*/)
	{
		Assert(CommandListData);
		CommandListData->CommandListManager->ExecuteCommandList(*this, {}, WaitForCompletion);
	}

	void D3D12CommandListHandle::AddTransitionBarrier(FD3D12Resource* pResource, D3D12_RESOURCE_STATES Before, D3D12_RESOURCE_STATES After, uint32_t Subresource)
	{
		Assert(CommandListData);
		CommandListData->ResourceBarrierBatcher.AddTransition(pResource->GetResource(), Before, After, Subresource);
		CommandListData->CurrentOwningContext->numBarriers++;
	}

	void D3D12CommandListHandle::AddUAVBarrier()
	{
		Assert(CommandListData);
		CommandListData->ResourceBarrierBatcher.AddUAV();
		CommandListData->CurrentOwningContext->numBarriers++;
	}

	void D3D12CommandListHandle::AddAliasingBarrier(FD3D12Resource* pResource)
	{
		Assert(CommandListData);
		CommandListData->ResourceBarrierBatcher.AddAliasingBarrier(pResource->GetResource());
		CommandListData->CurrentOwningContext->numBarriers++;
	}

	void inline D3D12CommandListHandle::D3D12CommandListData::FCommandListResourceState::ConditionalInitalize(FD3D12Resource* pResource, CResourceState& ResourceState)
	{
		// If there is no entry, all subresources should be in the resource's TBD state.
		// This means we need to have pending resource barrier(s).
		if (!ResourceState.CheckResourceStateInitalized())
		{
			ResourceState.Initialize(pResource->GetSubresourceCount());
			Assert(ResourceState.CheckResourceState(D3D12_RESOURCE_STATE_TBD));
		}

		Assert(ResourceState.CheckResourceStateInitalized());
	}

	CResourceState& D3D12CommandListHandle::D3D12CommandListData::FCommandListResourceState::GetResourceState(FD3D12Resource* pResource)
	{
		// Only certain resources should use this
		Assert(pResource->RequiresResourceStateTracking());

		//CResourceState& ResourceState = ResourceStates.FindOrAdd(pResource);
		CResourceState& ResourceState = ResourceStates[pResource];
		ConditionalInitalize(pResource, ResourceState);
		return ResourceState;
	}

	void D3D12CommandListHandle::D3D12CommandListData::FCommandListResourceState::Empty()
	{
		ResourceStates.clear();
	}

}