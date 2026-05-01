#include "D3D12/D3D12UniformBuffer.h"
#include "D3D12/D3D12Adapter.h"
#include "D3D12/D3D12WindowDevice.h"
#include "D3D12/D3D12CommandContext.h"
#include "D3D12/D3D12CommandList.h"
#include "D3D12/D3D12DirectCommandListManager.h"
#include "math/math.h"

namespace RenderCore
{
	struct D3D12UniformBufferPrivate
	{
		uint32_t ConstantBufferSize = 0;
		static constexpr uint32_t kRingSlots = 8;
		uint32_t RingStride = 0;
		uint32_t RingWriteIndex = 0;
		FAllocation RingAllocation{};
		bool RingAllocated = false;

		uint64_t SlotFenceValue[kRingSlots]{};
		ED3D12CommandQueueType SlotFenceQueue[kRingSlots]{};
		uint32_t PendingPublishMask = 0;
		ED3D12CommandQueueType SubmitQueueForSlot[kRingSlots]{};

		~D3D12UniformBufferPrivate()
		{
		}
	};

	D3D12UniformBuffer::D3D12UniformBuffer(std::weak_ptr<FD3D12Adapter> InParentAdapter)
		:FD3D12AdapterChild(InParentAdapter)
		,d_ptr(new D3D12UniformBufferPrivate())
	{

	}

	D3D12UniformBuffer::~D3D12UniformBuffer()
	{
		delete d_ptr;
	}

	bool D3D12UniformBuffer::CreateUniformBuffer(const void* Contents, uint32_t ConstantBufferSize)
	{
		C_P(D3D12UniformBuffer);
		if (!GetParentAdapter()->GetDevice()->GetDefaultCommandContext())
			return false;

		d->ConstantBufferSize = ConstantBufferSize;
		d->RingStride = (uint32_t)math::AlignUp(ConstantBufferSize, DEFAULT_ALIGN);

		// Dynamic uniform data from a bounded transient upload ring.
		// This avoids unbounded WC commit growth from "many small" allocations that touch new pages over time.
		auto& TransientUB = GetParentAdapter()->GetTransientUniformBufferAllocator();
		d->RingAllocation = TransientUB.Allocate((uint64_t)d->RingStride, DEFAULT_ALIGN);
		d->RingAllocated = (d->RingAllocation.CPU != nullptr && d->RingAllocation.GpuAddress != 0);
		d->RingWriteIndex = 0;

		if (Contents && d->RingAllocated)
			memcpy(d->RingAllocation.CPU, Contents, ConstantBufferSize);
		return d->RingAllocated;
	}

	uint32_t D3D12UniformBuffer::GetConstantBufferSize() const
	{
		C_P(const D3D12UniformBuffer);
		return d->ConstantBufferSize;
	}

	void* D3D12UniformBuffer::GetResourceBaseAddress() const
	{
		C_P(const D3D12UniformBuffer);
		if (!d->RingAllocated)
			return nullptr;
		return d->RingAllocation.CPU;
	}

	D3D12_GPU_VIRTUAL_ADDRESS D3D12UniformBuffer::GetGPUVirtualAddress() const
	{
		C_P(const D3D12UniformBuffer);
		if (!d->RingAllocated)
			return 0;
		return d->RingAllocation.GpuAddress;
	}

	void D3D12UniformBuffer::UpdateUniformBuffer(const void* Contents)
	{
		C_P(D3D12UniformBuffer);
		if (!d->RingAllocated || !Contents)
			return;

		// Allocate a fresh slice and publish it; reuse is fence-gated by the ring allocator.
		auto& TransientUB = GetParentAdapter()->GetTransientUniformBufferAllocator();
		FAllocation NewAlloc = TransientUB.Allocate((uint64_t)d->RingStride, DEFAULT_ALIGN);
		if (!NewAlloc.CPU || NewAlloc.GpuAddress == 0)
			return;

		memcpy(NewAlloc.CPU, Contents, d->ConstantBufferSize);
		d->RingAllocation = NewAlloc;
	}

	uint32_t D3D12UniformBuffer::GetActiveRingSlotIndex() const
	{
		C_P(const D3D12UniformBuffer);
		if (!d->RingAllocated)
			return 0;
		return 0;
	}

	void D3D12UniformBuffer::RecordGpuReferenceRingSlot(const D3D12CommandListHandle& cmdList, const std::shared_ptr<D3D12UniformBuffer>& selfRef)
	{
		C_P(D3D12UniformBuffer);
		if (!d->RingAllocated || !cmdList || !selfRef)
			return;
		Assert(selfRef.get() == this);
		const uint32_t slot = GetActiveRingSlotIndex();
		const ED3D12CommandQueueType q = cmdList.GetSubmitFenceQueueType();
		d->PendingPublishMask |= (1u << slot);
		d->SubmitQueueForSlot[slot] = q;
		cmdList.RegisterUniformBufferForSubmitFence(selfRef);
	}

	void D3D12UniformBuffer::OnCmdListSubmitFence(uint64_t fenceValue)
	{
		C_P(D3D12UniformBuffer);
		const uint32_t mask = d->PendingPublishMask;
		if (mask == 0)
			return;
		for (uint32_t i = 0; i < D3D12UniformBufferPrivate::kRingSlots; ++i)
		{
			if (mask & (1u << i))
			{
				d->SlotFenceValue[i] = fenceValue;
				d->SlotFenceQueue[i] = d->SubmitQueueForSlot[i];
			}
		}
		d->PendingPublishMask = 0;
	}

	void D3D12UniformBuffer::CancelPendingGpuFenceTags()
	{
		C_P(D3D12UniformBuffer);
		d->PendingPublishMask = 0;
	}

	void D3D12UniformBuffer::ResetGpuRingFences()
	{
		C_P(D3D12UniformBuffer);
		for (uint32_t i = 0; i < D3D12UniformBufferPrivate::kRingSlots; ++i)
		{
			d->SlotFenceValue[i] = 0;
			d->SlotFenceQueue[i] = ED3D12CommandQueueType::Default;
			d->SubmitQueueForSlot[i] = ED3D12CommandQueueType::Default;
		}
		d->PendingPublishMask = 0;
	}

}