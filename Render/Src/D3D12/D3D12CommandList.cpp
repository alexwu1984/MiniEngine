#include "D3D12/D3D12CommandList.h"
#include "D3D12/D3D12DirectCommandListManager.h"
#include "D3D12/D3D12CommandContext.h"

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
		assert(CommandAllocator.get() == nullptr);
		HRESULT hr = InDevice->CreateCommandAllocator(InType, IID_PPV_ARGS(CommandAllocator.get_init_ref()));
	}

	D3D12CommandListHandle::D3D12CommandListData::D3D12CommandListData(D3D12Device* ParentDevice, D3D12_COMMAND_LIST_TYPE InCommandListType, D3D12CommandAllocator& CommandAllocator, D3D12CommandListManager* InCommandListManager)
		:D3D12DeviceChild(ParentDevice)
		, CommandListType(InCommandListType)
		, CurrentCommandAllocator(&CommandAllocator)
		, CommandListManager(InCommandListManager)
		, CurrentGeneration(1)
		, LastCompleteGeneration(0)
		, IsClosed(false)
		, bShouldTrackStartEndTime(false)

	{

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
		VERIFYD3DRESULT(CommandList->Reset(CommandAllocator, nullptr));

		CurrentCommandAllocator = &CommandAllocator;
		IsClosed = false;

		// Indicate this command allocator is being used.
		CurrentCommandAllocator->IncrementPendingCommandLists();

		CleanupActiveGenerations();

		// Remove all pendering barriers from the command list
		PendingResourceBarriers.clear();


		// If this fails then some previous resource barriers were never submitted.
		assert(ResourceBarrierBatcher.GetBarriers().size() == 0);

	}

	D3D12CommandListHandle::D3D12CommandListData::~D3D12CommandListData()
	{

	}

	void D3D12CommandListHandle::Create(D3D12Device* ParentDevice, D3D12_COMMAND_LIST_TYPE CommandListType, D3D12CommandAllocator& CommandAllocator, D3D12CommandListManager* InCommandListManager)
	{
		assert(!CommandListData);
		CommandListData = new D3D12CommandListData(ParentDevice, CommandListType, CommandAllocator, InCommandListManager);
		CommandListData->AddRef();
	}

	void D3D12CommandListHandle::Execute(bool WaitForCompletion /*= false*/)
	{
		assert(CommandListData);
		CommandListData->CommandListManager->ExecuteCommandList(*this, WaitForCompletion);
	}

	void D3D12CommandListHandle::AddTransitionBarrier(D3D12Resource* pResource, D3D12_RESOURCE_STATES Before, D3D12_RESOURCE_STATES After, uint32_t Subresource)
	{
		assert(CommandListData);
		CommandListData->ResourceBarrierBatcher.AddTransition(pResource->GetResource(), Before, After, Subresource);
		CommandListData->CurrentOwningContext->numBarriers++;
	}

	void D3D12CommandListHandle::AddUAVBarrier()
	{
		assert(CommandListData);
		CommandListData->ResourceBarrierBatcher.AddUAV();
		CommandListData->CurrentOwningContext->numBarriers++;
	}

	void D3D12CommandListHandle::AddAliasingBarrier(D3D12Resource* pResource)
	{
		assert(CommandListData);
		CommandListData->ResourceBarrierBatcher.AddAliasingBarrier(pResource->GetResource());
		CommandListData->CurrentOwningContext->numBarriers++;
	}

	void inline D3D12CommandListHandle::D3D12CommandListData::FCommandListResourceState::ConditionalInitalize(D3D12Resource* pResource, CResourceState& ResourceState)
	{
		// If there is no entry, all subresources should be in the resource's TBD state.
		// This means we need to have pending resource barrier(s).
		if (!ResourceState.CheckResourceStateInitalized())
		{
			ResourceState.Initialize(pResource->GetSubresourceCount());
			assert(ResourceState.CheckResourceState(D3D12_RESOURCE_STATE_TBD));
		}

		assert(ResourceState.CheckResourceStateInitalized());
	}

	CResourceState& D3D12CommandListHandle::D3D12CommandListData::FCommandListResourceState::GetResourceState(D3D12Resource* pResource)
	{
		// Only certain resources should use this
		assert(pResource->RequiresResourceStateTracking());

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