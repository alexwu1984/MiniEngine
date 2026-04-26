#pragma once
#include "D3D12/D3D12RHICommon.h"
#include "D3D12/D3D12Allocation.h"
#include <d3d12.h>
#include <memory>

namespace RenderCore
{
	class FD3D12CommandListManager;
	class D3D12CommandContext;
	class FD3D12ResourceAllocator;
	class LinearAllocationPageManager;
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
		std::shared_ptr<D3D12CommandContext> GetDefaultCommandContext() const { return DefaultCommandContext;}
		std::shared_ptr<D3D12CommandContext> GetDefaultAsyncComputeContext() const { return AsyncComputeContext; }
		LinearAllocationPageManager& GetLinearPageManager(ELinearAllocatorType Type) const;

		FDynamicDescriptorHeapPoolsPerDevice& GetDynamicDescriptorHeapPools();

		D3D12_CPU_DESCRIPTOR_HANDLE AllocateDescriptor(D3D12_DESCRIPTOR_HEAP_TYPE Type, uint32_t Count = 1);
		FD3D12ResourceAllocator::FDescriptorAllocation AllocateDescriptorBlock(D3D12_DESCRIPTOR_HEAP_TYPE Type, uint32_t Count = 1);
		void FreeDescriptorBlock(D3D12_DESCRIPTOR_HEAP_TYPE Type, const FD3D12ResourceAllocator::FDescriptorAllocation& Allocation);
		uint32_t GetDescriptorSize(D3D12_DESCRIPTOR_HEAP_TYPE Type);;
		std::size_t GetCpuDescriptorHeapCount(D3D12_DESCRIPTOR_HEAP_TYPE Type) const { return DescriptorAllocator[Type] ? DescriptorAllocator[Type]->GetHeapCount() : 0; }
		std::size_t GetCpuDescriptorFreeBlockCount(D3D12_DESCRIPTOR_HEAP_TYPE Type) const { return DescriptorAllocator[Type] ? DescriptorAllocator[Type]->GetFreeBlockCount() : 0; }
		std::size_t GetCpuDescriptorGlobalPoolSize() const { return FD3D12ResourceAllocator::GetGlobalPoolSize(); }
		void BlockUntilIdle();

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
		std::shared_ptr<LinearAllocationPageManager> PageManager[ELinearAllocatorType::NumAllocatorTypes];

		std::unique_ptr<FDynamicDescriptorHeapPoolsPerDevice> DynamicDescriptorHeapPools;
	};
}