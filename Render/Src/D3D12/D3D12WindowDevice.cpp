#include "D3D12/D3D12WindowDevice.h"
#include "RHIPrivate/D3D12RHIPrivate.h"
#include "D3D12/D3D12Adapter.h"
#include "D3D12/D3D12CommandContext.h"
#include "D3D12/D3D12Allocation.h"
#include "D3D12/D3D12DescriptorCache.h"
#include "D3D12/D3D12BuddyAllocator.h"
#include "D3D12/D3D12UniformBuffer.h"

namespace RenderCore
{
	void FD3D12Device::EnqueuePendingCommandList(D3D12CommandListHandle&& List, ED3D12CommandQueueType QueueType)
	{
		if (!List)
			return;
		switch (QueueType)
		{
		case ED3D12CommandQueueType::Default: PendingCommandListsDefault.push_back(std::move(List)); break;
		case ED3D12CommandQueueType::Async:   PendingCommandListsAsync.push_back(std::move(List)); break;
		case ED3D12CommandQueueType::Copy:    PendingCommandListsCopy.push_back(std::move(List)); break;
		default:                              PendingCommandListsDefault.push_back(std::move(List)); break;
		}
	}

	bool FD3D12Device::HasPendingCommandLists(ED3D12CommandQueueType QueueType) const
	{
		switch (QueueType)
		{
		case ED3D12CommandQueueType::Default: return !PendingCommandListsDefault.empty();
		case ED3D12CommandQueueType::Async:   return !PendingCommandListsAsync.empty();
		case ED3D12CommandQueueType::Copy:    return !PendingCommandListsCopy.empty();
		default:                              return !PendingCommandListsDefault.empty();
		}
	}

	uint64_t FD3D12Device::ExecutePendingCommandLists(ED3D12CommandQueueType QueueType, bool WaitForCompletion /*= false*/)
	{
		FD3D12CommandListManager* Mgr = TryGetCommandListManager(QueueType);
		if (!Mgr)
			return 0;

		std::vector<D3D12CommandListHandle>* PendingPtr = nullptr;
		switch (QueueType)
		{
		case ED3D12CommandQueueType::Default: PendingPtr = &PendingCommandListsDefault; break;
		case ED3D12CommandQueueType::Async:   PendingPtr = &PendingCommandListsAsync; break;
		case ED3D12CommandQueueType::Copy:    PendingPtr = &PendingCommandListsCopy; break;
		default:                              PendingPtr = &PendingCommandListsDefault; break;
		}
		std::vector<D3D12CommandListHandle>& Pending = *PendingPtr;
		if (Pending.empty())
			return 0;

		const uint64_t Fence = Mgr->ExecuteCommandLists(Pending, [PendingCopy = Pending, QueueType](uint64_t FenceID) mutable {
			for (D3D12CommandListHandle& h : PendingCopy)
			{
				h.FlushPendingUniformBufferFenceTags(FenceID);
				h.CleanupTransientResources(FenceID, QueueType);
			}
			}, WaitForCompletion);
		Pending.clear();
		return Fence;
	}

	FD3D12Device::FD3D12Device(std::weak_ptr<FD3D12Adapter> InAdapter)
		:FD3D12AdapterChild(InAdapter)
		, DynamicDescriptorHeapPools(std::make_unique<FDynamicDescriptorHeapPoolsPerDevice>())
	{

	}

	FD3D12Device::~FD3D12Device()
	{

	}

	void FD3D12Device::Initialize()
	{
		CreateCommandContexts();
		InitPlatformSpecific();
		InitDescriptorAllocator();

		if(DefaultCommandContext)
			DefaultCommandContext->OpenCommandList();
		if(AsyncComputeContext)
			AsyncComputeContext->OpenCommandList();
	}

	void FD3D12Device::InitializeNullUniformBuffer()
	{
		if (NullUniformBuffer)
			return;
		std::vector<uint8_t> Zero(256u, 0u);
		NullUniformBuffer = std::make_shared<D3D12UniformBuffer>(GetParentAdapter());
		(void)NullUniformBuffer->CreateUniformBuffer(Zero.data(), (uint32_t)Zero.size());
	}

	void FD3D12Device::CreateCommandContexts()
	{
		DefaultCommandContext = std::make_shared<D3D12CommandContext>(this->shared_from_this(), true, false);
		DefaultCommandContext->Initialize();
		AsyncComputeContext = std::make_shared<D3D12CommandContext>(this->shared_from_this(), false, true);
		AsyncComputeContext->Initialize();
	}

	void FD3D12Device::InitPlatformSpecific()
	{
		CommandListManager = std::make_shared<FD3D12CommandListManager>(this->shared_from_this(), D3D12_COMMAND_LIST_TYPE_DIRECT, ED3D12CommandQueueType::Default);
		CopyCommandListManager = std::make_shared<FD3D12CommandListManager>(this->shared_from_this(), D3D12_COMMAND_LIST_TYPE_COPY, ED3D12CommandQueueType::Copy);
		AsyncCommandListManager = std::make_shared<FD3D12CommandListManager>(this->shared_from_this(), D3D12_COMMAND_LIST_TYPE_COMPUTE, ED3D12CommandQueueType::Async);

		CommandListManager->Create(L"3D Queue");
		CopyCommandListManager->Create(L"Copy Queue");
		AsyncCommandListManager->Create(L"Async Compute Queue", 0, 0);

		FastAllocator[0] = std::make_shared<FD3D12FastAllocator>(this->shared_from_this());
		FastAllocator[1] = std::make_shared<FD3D12FastAllocator>(this->shared_from_this());

		BuddyAllocator = std::make_unique<FD3D12BuddyAllocator>(this->shared_from_this(), eBuddyAllocationStrategy::kPlacedResourceStrategy);
		constexpr uint64_t kUploadBuddyHeapBytes = 128ull * 1024ull * 1024ull;
		if (!BuddyAllocator->Initialize(kUploadBuddyHeapBytes))
			BuddyAllocator.reset();
	}

	void FD3D12Device::InitDescriptorAllocator()
	{
		DescriptorAllocator[0] = std::make_shared<FD3D12ResourceAllocator>(this->shared_from_this(), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
		DescriptorAllocator[1] = std::make_shared<FD3D12ResourceAllocator>(this->shared_from_this(), D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER);
		DescriptorAllocator[2] = std::make_shared<FD3D12ResourceAllocator>(this->shared_from_this(), D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
		DescriptorAllocator[3] = std::make_shared<FD3D12ResourceAllocator>(this->shared_from_this(), D3D12_DESCRIPTOR_HEAP_TYPE_DSV);
	}

	void FD3D12Device::Cleanup()
	{
		if (DefaultCommandContext)
			DefaultCommandContext->Destroy();
		if (AsyncComputeContext)
			AsyncComputeContext->Destroy();
		DefaultCommandContext = {};
		AsyncComputeContext = {};

		// FastAllocator Destroy may run ProcessBuddyAllocatorDeferredFrees / Drain, which query fences on
		// CommandListManagers. Tear those managers down only after allocators no longer need them.
		if (FastAllocator[0])
		{
			FastAllocator[0]->Destroy();
			FastAllocator[0] = {};
		}
		if (FastAllocator[1])
		{
			FastAllocator[1]->Destroy();
			FastAllocator[1] = {};
		}

		BuddyAllocator.reset();

		if (CommandListManager)
			CommandListManager->Destroy();
		if (CopyCommandListManager)
			CopyCommandListManager->Destroy();
		if (AsyncCommandListManager)
			AsyncCommandListManager->Destroy();
		CommandListManager = {};
		CopyCommandListManager = {};
		AsyncCommandListManager = {};

		DescriptorAllocator[0] = {};
		DescriptorAllocator[1] = {};
		DescriptorAllocator[2] = {};
		DescriptorAllocator[3] = {};

		FD3D12ResourceAllocator::DestroyAll();
		if (DynamicDescriptorHeapPools != nullptr)
			DynamicDescriptorHeapPools->Clear();
	}

	FDynamicDescriptorHeapPoolsPerDevice& FD3D12Device::GetDynamicDescriptorHeapPools()
	{
		Assert(DynamicDescriptorHeapPools != nullptr);
		return *DynamicDescriptorHeapPools;
	}

	ID3D12Device* FD3D12Device::GetDevice()
	{
		return GetParentAdapter()->GetD3DDevice();
	}

	ID3D12CommandQueue* FD3D12Device::GetD3DCommandQueue(ED3D12CommandQueueType InQueueType /*= ED3D12CommandQueueType::Default*/) const
	{
		switch (InQueueType)
		{
		case ED3D12CommandQueueType::Default:
			Assert(CommandListManager->GetQueueType() == InQueueType);
			return CommandListManager->GetD3DCommandQueue();
		case ED3D12CommandQueueType::Async:
			Assert(AsyncCommandListManager->GetQueueType() == InQueueType);
			return AsyncCommandListManager->GetD3DCommandQueue();
		case ED3D12CommandQueueType::Copy:
			Assert(CopyCommandListManager->GetQueueType() == InQueueType);
			return CopyCommandListManager->GetD3DCommandQueue();
		default:
			Assert(false);
			return nullptr;
		}
	}

	FD3D12CommandListManager& FD3D12Device::GetCommandListManager(ED3D12CommandQueueType InQueueType /*= ED3D12CommandQueueType::Default*/) const
	{
		switch (InQueueType)
		{
		case ED3D12CommandQueueType::Default:
			Assert(CommandListManager->GetQueueType() == InQueueType);
			return *CommandListManager;
		case ED3D12CommandQueueType::Async:
			Assert(AsyncCommandListManager->GetQueueType() == InQueueType);
			return *AsyncCommandListManager;
		case ED3D12CommandQueueType::Copy:
			Assert(CopyCommandListManager->GetQueueType() == InQueueType);
			return *CopyCommandListManager;
		default:
			return *CommandListManager;
		}
	}

	FD3D12CommandListManager* FD3D12Device::TryGetCommandListManager(ED3D12CommandQueueType InQueueType) const noexcept
	{
		switch (InQueueType)
		{
		case ED3D12CommandQueueType::Default:
			return CommandListManager.get();
		case ED3D12CommandQueueType::Async:
			return AsyncCommandListManager.get();
		case ED3D12CommandQueueType::Copy:
			return CopyCommandListManager.get();
		default:
			return CommandListManager.get();
		}
	}

	FD3D12FastAllocator& FD3D12Device::GetFastAllocator(EFastAllocatorType InType) const
	{
		// Do not require both slots: Cleanup() destroys [0] then [1]. ~FD3D12FastAllocatorPage on the
		// second pool may call GetFastAllocator(Upload) while [0] is already nullptr.
		switch (InType)
		{
		case DefaultFastAllocator:
			Assert(FastAllocator[0].get());
			Assert(FastAllocator[0]->GetAllocatorType() == InType);
			return *FastAllocator[0];
		case UploadFastAllocator:
			Assert(FastAllocator[1].get());
			Assert(FastAllocator[1]->GetAllocatorType() == InType);
			return *FastAllocator[1];
		default:
			Assert(FastAllocator[0].get());
			return *FastAllocator[0];
		}
	}

	FD3D12ResourceAllocator::FDescriptorAllocation FD3D12Device::AllocateDescriptorBlock(D3D12_DESCRIPTOR_HEAP_TYPE Type, uint32_t Count /*= 1*/)
	{
		Assert(DescriptorAllocator[Type].get());
		return DescriptorAllocator[Type]->AllocateBlock(Count);
	}

	void FD3D12Device::FreeDescriptorBlock(D3D12_DESCRIPTOR_HEAP_TYPE Type, const FD3D12ResourceAllocator::FDescriptorAllocation& Allocation)
	{
		if (!DescriptorAllocator[Type].get())
			return;
		DescriptorAllocator[Type]->FreeBlock(Allocation);
	}

	uint32_t FD3D12Device::GetDescriptorSize(D3D12_DESCRIPTOR_HEAP_TYPE Type)
	{
		Assert(DescriptorAllocator[Type].get());
		return DescriptorAllocator[Type]->GetDescriptorSize();
	}

	void FD3D12Device::BlockUntilIdle()
	{
		// Teardown-safe idle wait:
		// Avoid submitting any command lists here. During shutdown, command list pointers can become
		// invalid due to teardown ordering/races, and ExecuteCommandLists may AV inside D3D12Core.
		// Signaling+waiting the queues is sufficient to ensure GPU idle.

		if (CommandListManager)
			CommandListManager->WaitForCommandQueueFlush();

		if (CopyCommandListManager)
			CopyCommandListManager->WaitForCommandQueueFlush();

		if (AsyncCommandListManager)
			AsyncCommandListManager->WaitForCommandQueueFlush();
	}

}