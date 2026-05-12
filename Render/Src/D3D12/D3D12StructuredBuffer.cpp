#include "D3D12/D3D12StructuredBuffer.h"
#include "D3D12/D3D12Adapter.h"
#include "D3D12/D3D12Allocation.h"
#include "D3D12/D3D12CommandContext.h"
#include "D3D12/D3D12Resource.h"
#include "D3D12/D3D12WindowDevice.h"
#include <cstring>

namespace RenderCore
{
	struct D3D12StructuredBufferPrivate
	{
		FD3D12Resource* Resource = nullptr;
		uint32_t Stride = 0;
		uint32_t Count = 0;
		uint32_t SizeBytes = 0;
		bool bDynamic = false;
		void* MappedPtr = nullptr;

		FD3D12ResourceAllocator::FDescriptorAllocation SrvAlloc{};
		D3D12_CPU_DESCRIPTOR_HANDLE SrvHandle{ D3D12_GPU_VIRTUAL_ADDRESS_NULL };

		~D3D12StructuredBufferPrivate()
		{
			if (Resource)
			{
				if (MappedPtr)
				{
					Resource->Unmap();
					MappedPtr = nullptr;
				}
				Resource->Release();
				Resource = nullptr;
			}
		}
	};

	D3D12StructuredBuffer::D3D12StructuredBuffer(std::weak_ptr<FD3D12Adapter> InParentAdapter)
		: FD3D12AdapterChild(InParentAdapter)
		, d_ptr(new D3D12StructuredBufferPrivate())
	{
	}

	D3D12StructuredBuffer::~D3D12StructuredBuffer()
	{
		// Return the offline SRV descriptor back to the allocator before the resource is released - otherwise every
		// per-frame structured buffer (clustered light table, etc.) would leak a CPU descriptor slot per frame.
		if (d_ptr && d_ptr->SrvAlloc.IsValid())
		{
			if (std::shared_ptr<FD3D12Adapter> Adapter = TryGetParentAdapter())
			{
				if (std::shared_ptr<FD3D12Device> Device = Adapter->GetDevice())
					Device->FreeDescriptorBlock(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, d_ptr->SrvAlloc);
			}
		}
		delete d_ptr;
	}

	std::shared_ptr<FD3D12Device> D3D12StructuredBuffer::GetParentDevice() const
	{
		std::shared_ptr<FD3D12Adapter> Adapter = TryGetParentAdapter();
		if (!Adapter)
			return {};
		return Adapter->GetDevice();
	}

	bool D3D12StructuredBuffer::CreateStructuredBuffer(uint32_t ElementStride, uint32_t ElementCount, EBufferUsageFlags Usage, const void* InitialData)
	{
		C_P(D3D12StructuredBuffer);
		if (ElementStride == 0 || ElementCount == 0)
			return false;

		d->Stride = ElementStride;
		d->Count = ElementCount;
		d->SizeBytes = ElementStride * ElementCount;
		d->bDynamic = (Usage & BUF_AnyDynamic) != 0;

		D3D12_RESOURCE_DESC ResDesc{};
		ResDesc.Alignment = 0;
		ResDesc.DepthOrArraySize = 1;
		ResDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
		ResDesc.Flags = D3D12_RESOURCE_FLAG_NONE;
		ResDesc.Format = DXGI_FORMAT_UNKNOWN;
		ResDesc.Width = (UINT64)d->SizeBytes;
		ResDesc.Height = 1;
		ResDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
		ResDesc.MipLevels = 1;
		ResDesc.SampleDesc.Count = 1;
		ResDesc.SampleDesc.Quality = 0;

		D3D12_HEAP_PROPERTIES HeapProps{};
		HeapProps.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
		HeapProps.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
		HeapProps.CreationNodeMask = 1;
		HeapProps.VisibleNodeMask = 1;

		std::shared_ptr<FD3D12Adapter> Adapter = GetParentAdapter();
		Assert(Adapter);
		HRESULT hr = E_FAIL;
		static int32_t sCounter = 0;
		const int32_t Counter = ++sCounter;
		if (d->bDynamic)
		{
			HeapProps.Type = D3D12_HEAP_TYPE_UPLOAD;
			std::wstring Name = core::formatw("W:", d->SizeBytes, "_StructuredUpload_", Counter);
			hr = Adapter->CreateCommittedResource(ResDesc, HeapProps, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, &d->Resource, Name.c_str());
			if (FAILED(hr) || !d->Resource)
				return false;
			// Persistent map: upload heaps allow concurrent CPU writes / GPU reads while mapped.
			d->MappedPtr = d->Resource->Map(nullptr);
			if (!d->MappedPtr)
				return false;
			if (InitialData)
				std::memcpy(d->MappedPtr, InitialData, d->SizeBytes);
		}
		else
		{
			HeapProps.Type = D3D12_HEAP_TYPE_DEFAULT;
			std::wstring Name = core::formatw("W:", d->SizeBytes, "_Structured_", Counter);
			hr = Adapter->CreateCommittedResource(ResDesc, HeapProps, D3D12_RESOURCE_STATE_COMMON, nullptr, &d->Resource, Name.c_str());
			if (FAILED(hr) || !d->Resource)
				return false;
			if (InitialData)
			{
				if (std::shared_ptr<D3D12CommandContext> Ctx = GetParentDevice()->GetDefaultCommandContext())
					Ctx->InitializeBuffer(d->Resource, InitialData, d->SizeBytes, 0);
			}
		}

		// One SRV descriptor (offline heap); persisted with the buffer and copied into the dynamic SRV table at Apply* time.
		std::shared_ptr<FD3D12Device> Device = GetParentDevice();
		Assert(Device);
		d->SrvAlloc = Device->AllocateDescriptorBlock(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 1);
		d->SrvHandle = d->SrvAlloc.Cpu;

		D3D12_SHADER_RESOURCE_VIEW_DESC SrvDesc{};
		SrvDesc.Format = DXGI_FORMAT_UNKNOWN;
		SrvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		SrvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
		SrvDesc.Buffer.FirstElement = 0;
		SrvDesc.Buffer.NumElements = d->Count;
		SrvDesc.Buffer.StructureByteStride = d->Stride;
		SrvDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;
		Device->GetDevice()->CreateShaderResourceView(d->Resource->GetResource(), &SrvDesc, d->SrvHandle);

		return true;
	}

	void D3D12StructuredBuffer::UpdateStructuredBuffer(const void* Contents, uint32_t SizeInBytes)
	{
		C_P(D3D12StructuredBuffer);
		if (!Contents || SizeInBytes == 0)
			return;
		if (!d->Resource)
			return;
		const uint32_t NumBytes = SizeInBytes > d->SizeBytes ? d->SizeBytes : SizeInBytes;

		if (d->bDynamic && d->MappedPtr)
		{
			// Single-slot upload heap: caller must ensure no in-flight GPU read of this region (ring buffering layered later).
			std::memcpy(d->MappedPtr, Contents, NumBytes);
			return;
		}

		// DEFAULT heap path: re-upload via GPU copy through the default context.
		if (std::shared_ptr<D3D12CommandContext> Ctx = GetParentDevice()->GetDefaultCommandContext())
			Ctx->InitializeBuffer(d->Resource, Contents, NumBytes, 0);
	}

	uint32_t D3D12StructuredBuffer::GetElementStride() const
	{
		C_P(const D3D12StructuredBuffer);
		return d->Stride;
	}

	uint32_t D3D12StructuredBuffer::GetElementCount() const
	{
		C_P(const D3D12StructuredBuffer);
		return d->Count;
	}

	D3D12_CPU_DESCRIPTOR_HANDLE D3D12StructuredBuffer::GetSRV() const
	{
		C_P(const D3D12StructuredBuffer);
		return d->SrvHandle;
	}

	D3D12_GPU_VIRTUAL_ADDRESS D3D12StructuredBuffer::GetGPUVirtualAddress() const
	{
		C_P(const D3D12StructuredBuffer);
		if (!d->Resource)
			return 0;
		return d->Resource->GetGPUVirtualAddress();
	}

	FD3D12Resource* D3D12StructuredBuffer::GetResource() const
	{
		C_P(const D3D12StructuredBuffer);
		return d->Resource;
	}

	bool D3D12StructuredBuffer::IsDynamic() const
	{
		C_P(const D3D12StructuredBuffer);
		return d->bDynamic;
	}
}
