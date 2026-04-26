#include "D3D12/D3D12Resource.h"
#include "D3D12/D3D12WindowDevice.h"

namespace RenderCore
{
	std::atomic_uint64_t FD3D12Resource::sLiveCountDefault{ 0 };
	std::atomic_uint64_t FD3D12Resource::sLiveCountUpload{ 0 };
	std::atomic_uint64_t FD3D12Resource::sLiveCountReadback{ 0 };
	std::atomic_uint64_t FD3D12Resource::sLiveBytesDefault{ 0 };
	std::atomic_uint64_t FD3D12Resource::sLiveBytesUpload{ 0 };
	std::atomic_uint64_t FD3D12Resource::sLiveBytesReadback{ 0 };
	std::atomic_uint64_t FD3D12Resource::sTotalCreateCount{ 0 };
	std::atomic_uint64_t FD3D12Resource::sTotalDestroyCount{ 0 };
	std::atomic_uint64_t FD3D12Resource::sTotalCreateBytes{ 0 };
	std::atomic_uint64_t FD3D12Resource::sTotalDestroyBytes{ 0 };

	FD3D12Resource::FD3D12Resource(std::weak_ptr<FD3D12Device> ParentDevice, ID3D12Resource* InResource,
								D3D12_RESOURCE_STATES InitialState, 
								D3D12_RESOURCE_DESC const& InDesc, 
								D3D12_HEAP_TYPE InHeapType /*= D3D12_HEAP_TYPE_DEFAULT*/)
		: FD3D12DeviceChild(ParentDevice)
		, Resource(InResource)
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
		TrackLiveAdd();
	}

	FD3D12Resource::~FD3D12Resource()
	{
		TrackLiveRemove();
	}

	void FD3D12Resource::DeferDelete()
	{

	}

}