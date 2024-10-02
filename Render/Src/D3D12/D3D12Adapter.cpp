#include "D3D12/D3D12Adapter.h"
#include "win/com_ptr.h"
#include "D3D12/D3D12RHI.h"

namespace RenderCore
{
	struct D3D12AdapterPrivate
	{
		// LDA setups have one ID3D12Device
		std::weak_ptr<D3D12DynamicRHI> RHI;
		win32::com_ptr<ID3D12Device> RootDevice;
		win32::com_ptr<ID3D12Device1> RootDevice1;
		win32::com_ptr<ID3D12Device2> RootDevice2;
		D3D12AdapterDesc Desc;
	};

	D3D12Adapter::D3D12Adapter(const D3D12AdapterDesc& desc)
		:d_ptr(new D3D12AdapterPrivate())
	{
		C_P(D3D12Adapter);
		d_ptr->Desc = desc;
	}

	D3D12Adapter::~D3D12Adapter()
	{
		delete d_ptr;
	}

	void D3D12Adapter::Initialize(std::weak_ptr<D3D12DynamicRHI> RHI)
	{
		C_P(D3D12Adapter);
		d->RHI = RHI;
	}

	void D3D12Adapter::InitializeDevices()
	{

	}

	void D3D12Adapter::InitializeRayTracing()
	{

	}

}