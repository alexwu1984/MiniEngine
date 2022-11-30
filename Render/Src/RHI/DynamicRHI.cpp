#include "RHI/DynamicRHI.h"

namespace RenderCore
{
	uint32_t GRHIVendorId = 0;
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