#include "D3D12/D3D12UniformBuffer.h"
#include "D3D12/D3D12Adapter.h"
#include "D3D12/D3D12WindowDevice.h"
#include "D3D12/D3D12CommandContext.h"

namespace RenderCore
{
	struct D3D12UniformBufferPrivate
	{
		uint32_t ConstantBufferSize = 0;
		LinearAllocationPage* AllocationPage = nullptr;
		D3D12_CPU_DESCRIPTOR_HANDLE hCBV{};

		~D3D12UniformBufferPrivate()
		{
			if(AllocationPage)
				AllocationPage->Release();
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

		const size_t AlignedSize = math::AlignUp(ConstantBufferSize, DEFAULT_ALIGN);

		LinearAllocationPageManager& PageManager = GetParentAdapter()->GetDevice()->GetLinearPageManager(ELinearAllocatorType::CpuWritable);
		d->AllocationPage = PageManager.CreateNewPage(AlignedSize);
		if (!d->AllocationPage)
			return false;

		memcpy(d->AllocationPage->GetResourceBaseAddress(), Contents, ConstantBufferSize);

		D3D12_CONSTANT_BUFFER_VIEW_DESC CBVDesc{};
		CBVDesc.BufferLocation = d->AllocationPage->GetGPUVirtualAddress();
		CBVDesc.SizeInBytes = (uint32_t)AlignedSize;

		d->hCBV = GetParentAdapter()->GetDevice()->AllocateDescriptor(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
		GetParentAdapter()->GetD3DDevice()->CreateConstantBufferView(&CBVDesc, d->hCBV);
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
		return d->AllocationPage->GetResourceBaseAddress();
	}

	D3D12_GPU_VIRTUAL_ADDRESS D3D12UniformBuffer::GetGPUVirtualAddress() const
	{
		C_P(const D3D12UniformBuffer);
		return d->AllocationPage->GetGPUVirtualAddress();
	}

	D3D12_CPU_DESCRIPTOR_HANDLE D3D12UniformBuffer::GetCPUHandle() const
	{
		C_P(const D3D12UniformBuffer);
		return d->hCBV;
	}

	void D3D12UniformBuffer::UpdateUniformBuffer(const void* Contents)
	{
		C_P(D3D12UniformBuffer);
		if (!d->AllocationPage->GetResourceBaseAddress())
			return;
		memcpy(d->AllocationPage->GetResourceBaseAddress(), Contents, d->ConstantBufferSize);
	}

}