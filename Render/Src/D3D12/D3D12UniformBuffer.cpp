#include "D3D12/D3D12UniformBuffer.h"
#include "D3D12/D3D12Adapter.h"
#include "D3D12/D3D12WindowDevice.h"
#include "D3D12/D3D12CommandContext.h"
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

		auto& Allocator = GetParentAdapter()->GetDevice()->GetDefaultCommandContext()->GetLinearAllocator(UploadFastAllocator);
		d->RingAllocation = Allocator.Allocate((size_t)d->RingStride * (size_t)D3D12UniformBufferPrivate::kRingSlots, DEFAULT_ALIGN);
		d->RingAllocated = true;
		// RingWriteIndex is the next slot to write; slot 0 holds the initial GPU-visible copy.
		d->RingWriteIndex = 1;

		if (Contents)
			memcpy(d->RingAllocation.CPU, Contents, ConstantBufferSize);
		return true;
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
		const uint32_t last = (d->RingWriteIndex + D3D12UniformBufferPrivate::kRingSlots - 1) % D3D12UniformBufferPrivate::kRingSlots;
		return (uint8_t*)d->RingAllocation.CPU + (size_t)last * (size_t)d->RingStride;
	}

	D3D12_GPU_VIRTUAL_ADDRESS D3D12UniformBuffer::GetGPUVirtualAddress() const
	{
		C_P(const D3D12UniformBuffer);
		if (!d->RingAllocated)
			return 0;
		const uint32_t last = (d->RingWriteIndex + D3D12UniformBufferPrivate::kRingSlots - 1) % D3D12UniformBufferPrivate::kRingSlots;
		return d->RingAllocation.GpuAddress + (uint64_t)last * (uint64_t)d->RingStride;
	}

	void D3D12UniformBuffer::UpdateUniformBuffer(const void* Contents)
	{
		C_P(D3D12UniformBuffer);
		if (!d->RingAllocated || !Contents)
			return;
		uint8_t* slot = (uint8_t*)d->RingAllocation.CPU + (size_t)d->RingWriteIndex * (size_t)d->RingStride;
		memcpy(slot, Contents, d->ConstantBufferSize);
		d->RingWriteIndex = (d->RingWriteIndex + 1) % D3D12UniformBufferPrivate::kRingSlots;
	}

}