#pragma once
#include "win/win32.h"
#include "RHI/RHIDefinitions.h"

namespace RenderCore
{
	enum class RHIAPIType
	{
		E_D3D11,
		E_D3D12,
	};

	class RHIViewPort;

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

		virtual std::shared_ptr< RHIViewPort> RHICreateViewport(void* WindowHandle, uint32_t SizeX, uint32_t SizeY, bool bIsFullscreen, EPixelFormat PreferredPixelFormat) { return nullptr; }
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
	std::shared_ptr<DynamicRHI> PlatformCreateDynamicRHI(RHIAPIType apiType);

	extern uint32_t GRHIVendorId;
	extern std::wstring GRHIAdapterName;
	extern uint32_t GRHIDeviceId;
	extern uint32_t GRHIDeviceRevision;
}

