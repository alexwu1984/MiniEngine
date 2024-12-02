#pragma once
#include "D3D12/D3D12Resource.h"

namespace RenderCore
{
	class D3D12ResourceAllocator : public FD3D12DeviceChild
	{
	public:

		D3D12ResourceAllocator(std::weak_ptr<FD3D12Device> ParentDevice,
			const std::wstring& Name,
			D3D12_HEAP_TYPE InHeapType,
			D3D12_RESOURCE_FLAGS Flags,
			uint32_t MaxSizeForPooling);

		~D3D12ResourceAllocator() = default;

		// Any allocation larger than this just gets straight up allocated (i.e. not pooled).
		// These large allocations should be infrequent so the CPU overhead should be minimal
		const uint32_t MaximumAllocationSizeForPooling;
		D3D12_RESOURCE_FLAGS ResourceFlags;

	protected:

		const std::wstring DebugName;

		bool Initialized;

		const D3D12_HEAP_TYPE HeapType;

		std::recursive_mutex CS;
	};
}