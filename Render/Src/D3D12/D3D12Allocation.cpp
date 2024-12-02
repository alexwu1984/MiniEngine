#include "D3D12/D3D12Allocation.h"
#include "D3D12/D3D12WindowDevice.h"

namespace RenderCore
{

	D3D12ResourceAllocator::D3D12ResourceAllocator(std::weak_ptr<FD3D12Device> ParentDevice,
												   const std::wstring& Name, 
												   D3D12_HEAP_TYPE InHeapType, D3D12_RESOURCE_FLAGS Flags, uint32_t MaxSizeForPooling)
		: FD3D12DeviceChild(ParentDevice)
		, MaximumAllocationSizeForPooling(MaxSizeForPooling)
		, ResourceFlags(Flags)
		, DebugName(Name)
		, Initialized(false)
		, HeapType(InHeapType)
	{

	}

}