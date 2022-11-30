#pragma once
#include "win/win32.h"

namespace RenderCore
{
	class DynamicRHI
	{
	public:
		DynamicRHI() = default;
		virtual ~DynamicRHI();

		/** Initializes the RHI; separate from IDynamicRHIModule::CreateRHI so that GDynamicRHI is set when it is called. */
		virtual void Init() = 0;

		/** Shutdown the RHI; handle shutdown and resource destruction before the RHI's actual destructor is called (so that all resources of the RHI are still available for shutdown). */
		virtual void Shutdown() = 0;

		virtual const TCHAR* GetName() = 0;
	};

	bool IsRHIDeviceAMD();

	// to trigger GPU specific optimizations and fallbacks
	bool IsRHIDeviceIntel();

	// to trigger GPU specific optimizations and fallbacks
	bool IsRHIDeviceNVIDIA();

	/**
*	Each platform that utilizes dynamic RHIs should implement this function
*	Called to create the instance of the dynamic RHI.
*/
	DynamicRHI* PlatformCreateDynamicRHI();
}

