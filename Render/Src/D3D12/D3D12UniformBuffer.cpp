#include "D3D12/D3D12UniformBuffer.h"
#include "D3D12/D3D12Adapter.h"
#include "D3D12/D3D12WindowDevice.h"
#include "D3D12/D3D12CommandContext.h"

namespace RenderCore
{
	struct D3D12UniformBufferPrivate
	{
		uint32_t ConstantBufferSize = 0;
		FAllocation Allocation;

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

		auto& Allocator = GetParentAdapter()->GetDevice()->GetDefaultCommandContext()->GetLinerAllocator(ELinearAllocatorType::CpuWritable);
		d->Allocation = Allocator.Allocate(ConstantBufferSize, DEFAULT_ALIGN);

		memcpy(d->Allocation.CPU, Contents, ConstantBufferSize);
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
		return  d->Allocation.CPU;
	}

	D3D12_GPU_VIRTUAL_ADDRESS D3D12UniformBuffer::GetGPUVirtualAddress() const
	{
		C_P(const D3D12UniformBuffer);
		return  d->Allocation.GpuAddress;
	}

	void D3D12UniformBuffer::UpdateUniformBuffer(const void* Contents)
	{
		C_P(D3D12UniformBuffer);
		auto& Allocator = GetParentAdapter()->GetDevice()->GetDefaultCommandContext()->GetLinerAllocator(ELinearAllocatorType::CpuWritable);
		d->Allocation = Allocator.Allocate(d->ConstantBufferSize, DEFAULT_ALIGN);

		memcpy(d->Allocation.CPU, Contents, d->ConstantBufferSize);
	}

}