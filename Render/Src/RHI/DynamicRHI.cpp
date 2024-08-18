#include "RHI/DynamicRHI.h"
#include "D3D11/D3D11RHI.h"

namespace RenderCore
{
	std::shared_ptr< IDynamicRHIModule> GRHIModule;
	std::shared_ptr<DynamicRHI> PlatformCreateDynamicRHI(RHIAPIType apiType)
	{
		if (apiType == RHIAPIType::E_D3D11)
		{
			GRHIModule = std::make_shared<D3D11DynamicRHIModule>();
			if (GRHIModule->IsSupported())
				return GRHIModule->CreateRHI();
		}
		else
		{
			return {};
		}
	}


	std::wstring GRHIAdapterName;
	uint32_t GRHIVendorId = 0;
	uint32_t GRHIDeviceId = 0;
	uint32_t GRHIDeviceRevision = 0;

	DynamicRHI::~DynamicRHI()
	{

	}

	bool IsRHIDeviceAMD()
	{
		Assert(GRHIVendorId != 0);
		// AMD's drivers tested on July 11 2013 have hitching problems with async resource streaming, setting single threaded for now until fixed.
		return GRHIVendorId == 0x1002;
	}

	bool IsRHIDeviceIntel()
	{
		Assert(GRHIVendorId != 0);
		// Intel GPUs are integrated and use both DedicatedVideoMemory and SharedSystemMemory.
		return GRHIVendorId == 0x8086;
	}

	bool IsRHIDeviceNVIDIA()
	{
		Assert(GRHIVendorId != 0);
		// NVIDIA GPUs are discrete and use DedicatedVideoMemory only.
		return GRHIVendorId == 0x10DE;
	}

}