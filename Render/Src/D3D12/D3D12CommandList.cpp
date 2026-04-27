#include "D3D12/D3D12CommandList.h"
#include "D3D12/D3D12DirectCommandListManager.h"
#include "D3D12/D3D12CommandContext.h"
#include "D3D12/D3D12WindowDevice.h"
#include "D3D12/D3D12StateCache.h"
#include "D3D12/D3D12UniformBuffer.h"

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
		, CurrentOwningContext(nullptr)
		, CurrentGeneration(1)
		, LastCompleteGeneration(0)
		, IsClosed(false)
		, bShouldTrackStartEndTime(false)
		, UploadLinearAllocator(UploadFastAllocator, ParentDevice)
		, DefaultLinearAllocator(DefaultFastAllocator, ParentDevice)

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

	void D3D12CommandListHandle::D3D12CommandListData::AddUniformBufferFenceTag(D3D12UniformBuffer* ub)
	{
		if (!ub)
			return;
		for (D3D12UniformBuffer* e : PendingUniformBuffersFenceTag)
		{
			if (e == ub)
				return;
		}
		PendingUniformBuffersFenceTag.push_back(ub);
	}

	void D3D12CommandListHandle::D3D12CommandListData::FlushPendingUniformBufferFenceTags(uint64_t fenceValue)
	{
		for (D3D12UniformBuffer* ub : PendingUniformBuffersFenceTag)
		{
			if (ub)
				ub->OnCmdListSubmitFence(fenceValue);
		}
		PendingUniformBuffersFenceTag.clear();
	}

	void D3D12CommandListHandle::D3D12CommandListData::CancelPendingUniformBufferFenceTags()
	{
		for (D3D12UniformBuffer* ub : PendingUniformBuffersFenceTag)
		{
			if (ub)
				ub->CancelPendingGpuFenceTags();
		}
		PendingUniformBuffersFenceTag.clear();
	}

	void D3D12CommandListHandle::D3D12CommandListData::Reset(D3D12CommandAllocator& CommandAllocator)
	{
		CancelPendingUniformBufferFenceTags();
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
		PendingResourceBarriers.shrink_to_fit();

		// Per-list tracked states are only needed until the list is submitted; after the allocator
		// is ready again, global resource state has been updated. Leaving this map populated causes
		// unbounded growth across frames for every distinct tracked resource pointer.
		TrackedResourceState.Empty();

		// If this fails then some previous resource barriers were never submitted.
		Assert(ResourceBarrierBatcher.GetBarriers().size() == 0);

	}

	D3D12CommandListHandle::D3D12CommandListData::~D3D12CommandListData()
	{
		CancelPendingUniformBufferFenceTags();
	}

	void D3D12CommandListHandle::Create(std::weak_ptr<FD3D12Device> ParentDevice, D3D12_COMMAND_LIST_TYPE CommandListType, D3D12CommandAllocator& CommandAllocator, FD3D12CommandListManager* InCommandListManager)
	{
		Assert(!CommandListData);
		CommandListData = new D3D12CommandListData(ParentDevice, CommandListType, CommandAllocator, InCommandListManager);
		CommandListData->AddRef();
	}

	uint64_t D3D12CommandListHandle::ExecuteAndClear(bool WaitForCompletion /*= false*/)
	{
		Assert(CommandListData);
		const uint64_t SignaledFenceValue =
			CommandListData->CommandListManager->ExecuteCommandList(*this, [this](uint64_t FenceID) {
			CommandListData->FlushPendingUniformBufferFenceTags(FenceID);
			const ED3D12CommandQueueType QueueType = CommandListData->CommandListManager->GetQueueType();
			CommandListData->UploadLinearAllocator.CleanupUsedPages(FenceID, QueueType);
			CommandListData->DefaultLinearAllocator.CleanupUsedPages(FenceID, QueueType);
			if (D3D12CommandContext* Ctx = GetCurrentOwningContext())
				Ctx->CleanupUsedHeaps(FenceID, QueueType);
			}, WaitForCompletion);
		return SignaledFenceValue;
	}

	void D3D12CommandListHandle::CleanupTransientResources(uint64_t FenceValue, ED3D12CommandQueueType QueueType)
	{
		Assert(CommandListData);
			CommandListData->UploadLinearAllocator.CleanupUsedPages(FenceValue, QueueType);
			CommandListData->DefaultLinearAllocator.CleanupUsedPages(FenceValue, QueueType);
		if (D3D12CommandContext* Ctx = GetCurrentOwningContext())
			Ctx->CleanupUsedHeaps(FenceValue, QueueType);
	}

	void D3D12CommandListHandle::Execute(bool WaitForCompletion /*= false*/)
	{
		// Keep a single submit path so we always run per-list cleanup (allocators, dynamic heaps, etc.).
		// This mirrors the intent of MiniEngine's CommandContext::Finish().
		(void)ExecuteAndClear(WaitForCompletion);
	}

	void D3D12CommandListHandle::RegisterUniformBufferForSubmitFence(D3D12UniformBuffer* ub) const
	{
		Assert(CommandListData);
		if (ub)
		{
			Assert(CommandListData->CommandListType == D3D12_COMMAND_LIST_TYPE_DIRECT
				|| CommandListData->CommandListType == D3D12_COMMAND_LIST_TYPE_COMPUTE);
		}
		CommandListData->AddUniformBufferFenceTag(ub);
	}

	void D3D12CommandListHandle::FlushPendingUniformBufferFenceTags(uint64_t fenceValue)
	{
		if (CommandListData)
			CommandListData->FlushPendingUniformBufferFenceTags(fenceValue);
	}

	void D3D12CommandListHandle::CancelPendingUniformBufferFenceTags()
	{
		if (CommandListData)
			CommandListData->CancelPendingUniformBufferFenceTags();
	}

	ED3D12CommandQueueType D3D12CommandListHandle::GetSubmitFenceQueueType() const
	{
		Assert(CommandListData);
		switch (CommandListData->CommandListType)
		{
		case D3D12_COMMAND_LIST_TYPE_DIRECT:
			return ED3D12CommandQueueType::Default;
		case D3D12_COMMAND_LIST_TYPE_COMPUTE:
			return ED3D12CommandQueueType::Async;
		case D3D12_COMMAND_LIST_TYPE_COPY:
			return ED3D12CommandQueueType::Copy;
		default:
			return ED3D12CommandQueueType::Default;
		}
	}

	void D3D12CommandListHandle::SetGraphicsRootConstantBufferViewUniform(UINT RootParameterIndex, D3D12UniformBuffer* UniformBuffer) const
	{
		Assert(CommandListData);
		Assert(CommandListData->CommandListType == D3D12_COMMAND_LIST_TYPE_DIRECT);
		if (UniformBuffer)
			UniformBuffer->RecordGpuReferenceRingSlot(*this);
		const D3D12_GPU_VIRTUAL_ADDRESS va = UniformBuffer ? UniformBuffer->GetGPUVirtualAddress() : D3D12_GPU_VIRTUAL_ADDRESS_NULL;
		GraphicsCommandList()->SetGraphicsRootConstantBufferView(RootParameterIndex, va);
	}

	void D3D12CommandListHandle::SetComputeRootConstantBufferViewUniform(UINT RootParameterIndex, D3D12UniformBuffer* UniformBuffer) const
	{
		Assert(CommandListData);
		Assert(CommandListData->CommandListType == D3D12_COMMAND_LIST_TYPE_DIRECT
			|| CommandListData->CommandListType == D3D12_COMMAND_LIST_TYPE_COMPUTE);
		if (UniformBuffer)
			UniformBuffer->RecordGpuReferenceRingSlot(*this);
		const D3D12_GPU_VIRTUAL_ADDRESS va = UniformBuffer ? UniformBuffer->GetGPUVirtualAddress() : D3D12_GPU_VIRTUAL_ADDRESS_NULL;
		GraphicsCommandList()->SetComputeRootConstantBufferView(RootParameterIndex, va);
	}

	void D3D12CommandListHandle::SetGraphicsRoot32BitConstantsFromUniform(UINT RootParameterIndex, UINT Num32BitValues, D3D12UniformBuffer* UniformBuffer, UINT DestOffsetIn32BitValues) const
	{
		Assert(CommandListData);
		Assert(CommandListData->CommandListType == D3D12_COMMAND_LIST_TYPE_DIRECT);
		if (!UniformBuffer || Num32BitValues == 0)
			return;
		void* cpu = UniformBuffer->GetResourceBaseAddress();
		if (!cpu)
			return;
		UniformBuffer->RecordGpuReferenceRingSlot(*this);
		GraphicsCommandList()->SetGraphicsRoot32BitConstants(RootParameterIndex, Num32BitValues, cpu, DestOffsetIn32BitValues);
	}

	void D3D12CommandListHandle::SetComputeRoot32BitConstantsFromUniform(UINT RootParameterIndex, UINT Num32BitValues, D3D12UniformBuffer* UniformBuffer, UINT DestOffsetIn32BitValues) const
	{
		Assert(CommandListData);
		Assert(CommandListData->CommandListType == D3D12_COMMAND_LIST_TYPE_DIRECT
			|| CommandListData->CommandListType == D3D12_COMMAND_LIST_TYPE_COMPUTE);
		if (!UniformBuffer || Num32BitValues == 0)
			return;
		void* cpu = UniformBuffer->GetResourceBaseAddress();
		if (!cpu)
			return;
		UniformBuffer->RecordGpuReferenceRingSlot(*this);
		GraphicsCommandList()->SetComputeRoot32BitConstants(RootParameterIndex, Num32BitValues, cpu, DestOffsetIn32BitValues);
	}

	void D3D12CommandListHandle::AddTransitionBarrier(FD3D12Resource* pResource, D3D12_RESOURCE_STATES Before, D3D12_RESOURCE_STATES After, uint32_t Subresource)
	{
		Assert(CommandListData);
		CommandListData->ResourceBarrierBatcher.AddTransition(pResource->GetResource(), Before, After, Subresource);
		Render::D3D12CallStats::AddResourceBarriers(1);
		CommandListData->CurrentOwningContext->numBarriers++;
	}

	void D3D12CommandListHandle::AddUAVBarrier()
	{
		Assert(CommandListData);
		CommandListData->ResourceBarrierBatcher.AddUAV();
		Render::D3D12CallStats::AddResourceBarriers(1);
		CommandListData->CurrentOwningContext->numBarriers++;
	}

	void D3D12CommandListHandle::AddAliasingBarrier(FD3D12Resource* pResource)
	{
		Assert(CommandListData);
		CommandListData->ResourceBarrierBatcher.AddAliasingBarrier(pResource->GetResource());
		Render::D3D12CallStats::AddResourceBarriers(1);
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
		// Return the map's tree bookkeeping to the heap; clear() alone can leave a large internal
		// structure allocated across Present/Flush cycles (shows up as many small heap allocs in VS).
		std::map<FD3D12Resource*, CResourceState> empty;
		ResourceStates.swap(empty);
	}

}