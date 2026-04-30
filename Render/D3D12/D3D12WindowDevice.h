#pragma once
#include "D3D12/D3D12RHICommon.h"
#include "D3D12/D3D12CommandList.h"
#include "D3D12/D3D12Allocation.h"
#include <d3d12.h>
#include <memory>
#include <mutex>
#include <vector>

namespace RenderCore
{
	class FD3D12CommandListManager;
	class D3D12CommandContext;
		class D3D12UniformBuffer;
	class FD3D12ResourceAllocator;
	class FD3D12BuddyAllocator;
	struct FDynamicDescriptorHeapPoolsPerDevice;

	class FD3D12Device :public std::enable_shared_from_this<FD3D12Device>,public FD3D12AdapterChild
	{
	public:
		FD3D12Device(std::weak_ptr<FD3D12Adapter> InAdapter);
		virtual ~FD3D12Device();

		/** Initialized members*/
		void Initialize();
		/**
		* Cleanup the device.
		* This function must be called from the main game thread.
		*/
		virtual void Cleanup();

		ID3D12Device* GetDevice();

		ID3D12CommandQueue* GetD3DCommandQueue(ED3D12CommandQueueType InQueueType = ED3D12CommandQueueType::Default) const;
		FD3D12CommandListManager& GetCommandListManager(ED3D12CommandQueueType InQueueType = ED3D12CommandQueueType::Default) const;
		/** nullptr if the manager was not created yet or already torn down (e.g. partial Cleanup order). */
		FD3D12CommandListManager* TryGetCommandListManager(ED3D12CommandQueueType InQueueType) const noexcept;
		std::shared_ptr<D3D12CommandContext> GetDefaultCommandContext() const { return DefaultCommandContext;}
		std::shared_ptr<D3D12CommandContext> GetDefaultAsyncComputeContext() const { return AsyncComputeContext; }
		FD3D12FastAllocator& GetFastAllocator(EFastAllocatorType Type) const;
		FD3D12BuddyAllocator* GetBuddyAllocator() const { return BuddyAllocator.get(); }

		FDynamicDescriptorHeapPoolsPerDevice& GetDynamicDescriptorHeapPools();

		FD3D12ResourceAllocator::FDescriptorAllocation AllocateDescriptorBlock(D3D12_DESCRIPTOR_HEAP_TYPE Type, uint32_t Count = 1);
		void FreeDescriptorBlock(D3D12_DESCRIPTOR_HEAP_TYPE Type, const FD3D12ResourceAllocator::FDescriptorAllocation& Allocation);
		uint32_t GetDescriptorSize(D3D12_DESCRIPTOR_HEAP_TYPE Type);;
		std::size_t GetCpuDescriptorHeapCount(D3D12_DESCRIPTOR_HEAP_TYPE Type) const { return DescriptorAllocator[Type] ? DescriptorAllocator[Type]->GetHeapCount() : 0; }
		std::size_t GetCpuDescriptorFreeBlockCount(D3D12_DESCRIPTOR_HEAP_TYPE Type) const { return DescriptorAllocator[Type] ? DescriptorAllocator[Type]->GetFreeBlockCount() : 0; }
		std::size_t GetCpuDescriptorGlobalPoolSize() const { return FD3D12ResourceAllocator::GetGlobalPoolSize(); }
		void BlockUntilIdle();

		/** D3D12DynamicRHI::RHIBeginFrame: frame fence catch-up + ping-pong command-list ready pools. */
		void NotifyRHIRecordingFrameBegin();

		/** Max transitions per ID3D12GraphicsCommandList::ResourceBarrier when batching pending PRBs (here: int32 max, matches typical Windows D3D12 RHI path). */
		uint32_t GetResourceBarrierBatchSizeLimit() const;

		// Global null resources for safety (GPU validation / uninitialized root args).
		// Not a replacement for correct binding; only prevents undefined reads when shaders declare resources.
		std::shared_ptr<D3D12UniformBuffer> GetNullUniformBuffer() const { return NullUniformBuffer; }
		void InitializeNullUniformBuffer();
		/** Creates shared null SRV/UAV descriptors in the device CPU pool during Initialize (safe to read from the RHI thread). */
		void InitializeNullSrvUavDescriptors();
		D3D12_CPU_DESCRIPTOR_HANDLE GetNullSrvCpu() const noexcept { return NullSrvCpu; }
		/** Null SRV with ViewDimension TEXTURECUBE (GBV: cube slots cannot use 2D null SRV). */
		D3D12_CPU_DESCRIPTOR_HANDLE GetNullSrvCubeCpu() const noexcept { return NullSrvCubeCpu; }
		D3D12_CPU_DESCRIPTOR_HANDLE GetNullUavCpu() const noexcept { return NullUavCpu; }

		// Accumulate command lists per queue and submit in a batch.
		void EnqueuePendingCommandList(D3D12CommandListHandle&& List, ED3D12CommandQueueType QueueType);
		uint64_t ExecutePendingCommandLists(ED3D12CommandQueueType QueueType, bool WaitForCompletion = false);
		bool HasPendingCommandLists(ED3D12CommandQueueType QueueType) const;

	private:
		void CreateCommandContexts();
		void InitPlatformSpecific();
		void InitDescriptorAllocator();

	private:
		/** A pool of command lists we can cycle through for the global D3D device */
		std::shared_ptr<FD3D12CommandListManager> CommandListManager;
		std::shared_ptr<FD3D12CommandListManager> CopyCommandListManager;
		std::shared_ptr<FD3D12CommandListManager> AsyncCommandListManager;
		std::shared_ptr<D3D12CommandContext> DefaultCommandContext;
		std::shared_ptr<D3D12CommandContext> AsyncComputeContext;

		std::shared_ptr<FD3D12ResourceAllocator> DescriptorAllocator[D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES];
		std::shared_ptr<FD3D12FastAllocator> FastAllocator[FastAllocator_Num];

		std::unique_ptr<FDynamicDescriptorHeapPoolsPerDevice> DynamicDescriptorHeapPools;
		std::unique_ptr<FD3D12BuddyAllocator> BuddyAllocator;

		std::vector<D3D12CommandListHandle> PendingCommandListsDefault;
		std::vector<D3D12CommandListHandle> PendingCommandListsAsync;
		std::vector<D3D12CommandListHandle> PendingCommandListsCopy;
		/** Serializes enqueue / peek / drain of pending lists (future RHI thread vs recorder). */
		mutable std::mutex PendingCommandListsMutex;

		std::shared_ptr<D3D12UniformBuffer> NullUniformBuffer;

		FD3D12ResourceAllocator::FDescriptorAllocation NullSrvAlloc{};
		D3D12_CPU_DESCRIPTOR_HANDLE NullSrvCpu{};
		FD3D12ResourceAllocator::FDescriptorAllocation NullSrvCubeAlloc{};
		D3D12_CPU_DESCRIPTOR_HANDLE NullSrvCubeCpu{};
		FD3D12ResourceAllocator::FDescriptorAllocation NullUavAlloc{};
		D3D12_CPU_DESCRIPTOR_HANDLE NullUavCpu{};
	};
}