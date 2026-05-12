#include "D3D12/D3D12StructuredBuffer.h"
#include "D3D12/D3D12Adapter.h"
#include "D3D12/D3D12Allocation.h"
#include "D3D12/D3D12CommandContext.h"
#include "D3D12/D3D12Resource.h"
#include "D3D12/D3D12WindowDevice.h"
#include <array>
#include <cstring>

namespace RenderCore
{
	namespace
	{
		// Sized for the worst case among supported D3D12 RHIs (RHIRecommendedParallelFrameResourceSlots == 3).
		// One Update->Draw round-trip per frame keeps GPU reads on slot N safe while CPU writes slot (N+1)%kDynamicRingSlots.
		constexpr uint32_t kDynamicRingSlots = 3u;
	}

	struct D3D12StructuredBufferPrivate
	{
		FD3D12Resource* Resource = nullptr;
		uint32_t Stride = 0;
		uint32_t Count = 0;
		/** Per-slot payload size (== Stride * Count). Total resource size for dynamic = SlotSizeBytes * RingSlotCount. */
		uint32_t SlotSizeBytes = 0;
		bool bDynamic = false;
		bool bHasUAV = false;
		/** Persistent map base; per-slot CPU pointer derived via SlotSizeBytes offset. Static path leaves null. */
		uint8_t* MappedPtr = nullptr;

		uint32_t RingSlotCount = 1u;
		uint32_t CurrentSlot = 0u;

		/** Block of `RingSlotCount` contiguous offline SRV descriptors. Index `CurrentSlot` is the one consumed by GetSRV. */
		FD3D12ResourceAllocator::FDescriptorAllocation SrvAlloc{};
		std::array<D3D12_CPU_DESCRIPTOR_HANDLE, kDynamicRingSlots> SrvHandles{};
		/** Single offline UAV descriptor; only valid for BUF_UnorderedAccess buffers (always DEFAULT heap → no ring). */
		FD3D12ResourceAllocator::FDescriptorAllocation UavAlloc{};
		D3D12_CPU_DESCRIPTOR_HANDLE UavHandle{ D3D12_GPU_VIRTUAL_ADDRESS_NULL };

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
		// Return the offline SRV/UAV descriptors back to the allocator before the resource is released - otherwise
		// every per-frame structured buffer (clustered light table, etc.) would leak CPU descriptor slots per frame.
		if (d_ptr)
		{
			if (std::shared_ptr<FD3D12Adapter> Adapter = TryGetParentAdapter())
			{
				if (std::shared_ptr<FD3D12Device> Device = Adapter->GetDevice())
				{
					if (d_ptr->SrvAlloc.IsValid())
						Device->FreeDescriptorBlock(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, d_ptr->SrvAlloc);
					if (d_ptr->UavAlloc.IsValid())
						Device->FreeDescriptorBlock(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, d_ptr->UavAlloc);
				}
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
		d->SlotSizeBytes = ElementStride * ElementCount;
		d->bDynamic = (Usage & BUF_AnyDynamic) != 0;
		d->bHasUAV = (Usage & BUF_UnorderedAccess) != 0;
		// UPLOAD heap can't be UAV-bound; reject the combo at the boundary so the caller sees the error here, not
		// inside the D3D validation layer at first dispatch.
		if (d->bDynamic && d->bHasUAV)
			return false;
		d->RingSlotCount = d->bDynamic ? kDynamicRingSlots : 1u;
		d->CurrentSlot = 0u;

		const uint32_t TotalSizeBytes = d->SlotSizeBytes * d->RingSlotCount;

		D3D12_RESOURCE_DESC ResDesc{};
		ResDesc.Alignment = 0;
		ResDesc.DepthOrArraySize = 1;
		ResDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
		ResDesc.Flags = d->bHasUAV ? D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS : D3D12_RESOURCE_FLAG_NONE;
		ResDesc.Format = DXGI_FORMAT_UNKNOWN;
		ResDesc.Width = (UINT64)TotalSizeBytes;
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
		Assert(Adapter.get());
		HRESULT hr = E_FAIL;
		static int32_t sCounter = 0;
		const int32_t Counter = ++sCounter;
		if (d->bDynamic)
		{
			HeapProps.Type = D3D12_HEAP_TYPE_UPLOAD;
			std::wstring Name = core::formatw("W:", TotalSizeBytes, "_StructuredUploadRing", d->RingSlotCount, "_", Counter);
			hr = Adapter->CreateCommittedResource(ResDesc, HeapProps, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, &d->Resource, Name.c_str());
			if (FAILED(hr) || !d->Resource)
				return false;
			d->MappedPtr = reinterpret_cast<uint8_t*>(d->Resource->Map(nullptr));
			if (!d->MappedPtr)
				return false;
			// Seed every slot with InitialData so any consumer that binds before the first Update() sees deterministic
			// payload regardless of which ring slot CurrentSlot starts on.
			if (InitialData)
			{
				for (uint32_t Slot = 0; Slot < d->RingSlotCount; ++Slot)
					std::memcpy(d->MappedPtr + (size_t)Slot * d->SlotSizeBytes, InitialData, d->SlotSizeBytes);
			}
		}
		else
		{
			HeapProps.Type = D3D12_HEAP_TYPE_DEFAULT;
			// Start UAV-capable buffers in COMMON so the first SRV / UAV transition picks up a known state; pure SRV
			// buffers also start in COMMON and transition to PIXEL_SHADER_RESOURCE on first bind (existing path).
			const D3D12_RESOURCE_STATES InitialState = D3D12_RESOURCE_STATE_COMMON;
			std::wstring Name = d->bHasUAV
				? core::formatw("W:", d->SlotSizeBytes, "_StructuredUAV_", Counter)
				: core::formatw("W:", d->SlotSizeBytes, "_Structured_", Counter);
			hr = Adapter->CreateCommittedResource(ResDesc, HeapProps, InitialState, nullptr, &d->Resource, Name.c_str());
			if (FAILED(hr) || !d->Resource)
				return false;
			if (InitialData)
			{
				if (std::shared_ptr<D3D12CommandContext> Ctx = GetParentDevice()->GetDefaultCommandContext())
					Ctx->InitializeBuffer(d->Resource, InitialData, d->SlotSizeBytes, 0);
			}
		}

		// One offline SRV descriptor per ring slot (offset into the same buffer). FD3D12StateCache copies the current
		// slot's CPU handle into the dynamic SRV table at Apply* time, so structured buffers share register space with textures.
		std::shared_ptr<FD3D12Device> Device = GetParentDevice();
		Assert(Device.get());
		d->SrvAlloc = Device->AllocateDescriptorBlock(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, d->RingSlotCount);
		const UINT IncrementSize = Device->GetDevice()->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
		for (uint32_t Slot = 0; Slot < d->RingSlotCount; ++Slot)
		{
			D3D12_CPU_DESCRIPTOR_HANDLE Handle{};
			Handle.ptr = d->SrvAlloc.Cpu.ptr + (SIZE_T)Slot * IncrementSize;
			d->SrvHandles[Slot] = Handle;

			D3D12_SHADER_RESOURCE_VIEW_DESC SrvDesc{};
			SrvDesc.Format = DXGI_FORMAT_UNKNOWN;
			SrvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
			SrvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
			SrvDesc.Buffer.FirstElement = (UINT64)Slot * d->Count;
			SrvDesc.Buffer.NumElements = d->Count;
			SrvDesc.Buffer.StructureByteStride = d->Stride;
			SrvDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;
			Device->GetDevice()->CreateShaderResourceView(d->Resource->GetResource(), &SrvDesc, Handle);
		}

		if (d->bHasUAV)
		{
			d->UavAlloc = Device->AllocateDescriptorBlock(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 1);
			d->UavHandle = d->UavAlloc.Cpu;
			D3D12_UNORDERED_ACCESS_VIEW_DESC UavDesc{};
			UavDesc.Format = DXGI_FORMAT_UNKNOWN;
			UavDesc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
			UavDesc.Buffer.FirstElement = 0;
			UavDesc.Buffer.NumElements = d->Count;
			UavDesc.Buffer.StructureByteStride = d->Stride;
			UavDesc.Buffer.CounterOffsetInBytes = 0;
			UavDesc.Buffer.Flags = D3D12_BUFFER_UAV_FLAG_NONE;
			Device->GetDevice()->CreateUnorderedAccessView(d->Resource->GetResource(), nullptr, &UavDesc, d->UavHandle);
		}

		return true;
	}

	void D3D12StructuredBuffer::UpdateStructuredBuffer(const void* Contents, uint32_t SizeInBytes)
	{
		C_P(D3D12StructuredBuffer);
		if (!Contents || SizeInBytes == 0)
			return;
		if (!d->Resource)
			return;
		const uint32_t NumBytes = SizeInBytes > d->SlotSizeBytes ? d->SlotSizeBytes : SizeInBytes;

		if (d->bDynamic && d->MappedPtr)
		{
			// Advance ring slot first so GetSRV() returns the slot we are about to write. The pipeline never reads
			// the same slot the CPU is touching while RHIRecommendedParallelFrameResourceSlots-1 frames are still in flight.
			d->CurrentSlot = (d->CurrentSlot + 1u) % d->RingSlotCount;
			uint8_t* DstPtr = d->MappedPtr + (size_t)d->CurrentSlot * d->SlotSizeBytes;
			std::memcpy(DstPtr, Contents, NumBytes);
			return;
		}

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
		// Match the slot UpdateStructuredBuffer just wrote so the SRV the dynamic descriptor table picks up corresponds
		// to the freshly written data; Static buffers always return slot 0.
		return d->SrvHandles[d->CurrentSlot];
	}

	D3D12_CPU_DESCRIPTOR_HANDLE D3D12StructuredBuffer::GetUAV() const
	{
		C_P(const D3D12StructuredBuffer);
		return d->UavHandle;
	}

	bool D3D12StructuredBuffer::HasUAV() const
	{
		C_P(const D3D12StructuredBuffer);
		return d->bHasUAV;
	}

	D3D12_GPU_VIRTUAL_ADDRESS D3D12StructuredBuffer::GetGPUVirtualAddress() const
	{
		C_P(const D3D12StructuredBuffer);
		if (!d->Resource)
			return 0;
		return d->Resource->GetGPUVirtualAddress() + (UINT64)d->CurrentSlot * d->SlotSizeBytes;
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
