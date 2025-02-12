#include "D3D12/D3D12Resource.h"
#include "D3D12/D3D12WindowDevice.h"

namespace RenderCore
{

	FD3D12Resource::FD3D12Resource(std::weak_ptr<FD3D12Device> ParentDevice, 
								D3D12MA::Allocation* InAllocation, ID3D12Resource* InResource,
								D3D12_RESOURCE_STATES InitialState, 
								D3D12_RESOURCE_DESC const& InDesc, 
								D3D12_HEAP_TYPE InHeapType /*= D3D12_HEAP_TYPE_DEFAULT*/)
		: FD3D12DeviceChild(ParentDevice)
		, Resource(InResource)
		, Allocation(InAllocation)
		, Desc(InDesc)
		, PlaneCount(RenderCore::GetPlaneCount(InDesc.Format))
		, SubresourceCount(0)
		, DefaultResourceState(D3D12_RESOURCE_STATE_TBD)
		, bRequiresResourceStateTracking(true)
		, bDepthStencil(false)
		, bDeferDelete(false)
		, HeapType(InHeapType)
		, GPUVirtualAddress(0)
		, ResourceBaseAddress(nullptr)
	{
		if (Resource
			&& Desc.Dimension == D3D12_RESOURCE_DIMENSION_BUFFER
			)
		{
			GPUVirtualAddress = Resource->GetGPUVirtualAddress();
		}

		InitalizeResourceState(InitialState);
	}

	FD3D12Resource::~FD3D12Resource()
	{
		Resource.reset();
		Allocation.reset();
	}

	void FD3D12Resource::DeferDelete()
	{

	}

}