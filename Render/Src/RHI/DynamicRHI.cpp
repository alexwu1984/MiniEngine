#include "RHI/DynamicRHI.h"
#include "D3D11/D3D11RHI.h"
#include "D3D12/D3D12RHI.h"
#include "core/logger.h"
#include <atomic>

namespace RenderCore
{
	namespace
	{
		std::atomic<bool> GRHIFatalDeviceLossForShell{ false };
		std::atomic<bool> GRHILoggedFatalGpuOnce{ false };
		std::atomic<uint32_t> GRHIShellMessageThreadId{ 0 };

		static void RHI_PostQuitToShellThread()
		{
			const uint32_t tid = GRHIShellMessageThreadId.load(std::memory_order_relaxed);
			if (tid)
				::PostThreadMessageW(static_cast<DWORD>(tid), WM_QUIT, 1, 0);
		}
	}

	bool RHI_HasFatalDeviceLossForShell()
	{
		return GRHIFatalDeviceLossForShell.load(std::memory_order_relaxed);
	}

	void RHI_SetShellMessageThreadIdForFatalDeviceLossQuit(uint32_t win32ThreadId)
	{
		GRHIShellMessageThreadId.store(win32ThreadId, std::memory_order_relaxed);
	}

	void RHI_NotifyFatalGpuDeviceLoss(const wchar_t* apiLabel, HRESULT hrPresentOrZero, HRESULT hrDeviceRemovedReason)
	{
		GRHIFatalDeviceLossForShell.store(true, std::memory_order_relaxed);
		bool expected = false;
		if (GRHILoggedFatalGpuOnce.compare_exchange_strong(expected, true, std::memory_order_relaxed))
		{
			core::LOG(core::log_e::log_err,
					  L"%s: GPU device removed (0x%08X / 0x%08X)",
					  apiLabel ? apiLabel : L"RHI",
					  (unsigned)hrPresentOrZero,
					  (unsigned)hrDeviceRemovedReason);
		}
		RHI_PostQuitToShellThread();
	}

	std::shared_ptr< IDynamicRHIModule> GRHIModule;
	std::shared_ptr<DynamicRHI> PlatformCreateDynamicRHI(RHIAPIType apiType)
	{
		if (apiType == RHIAPIType::E_D3D11)
		{
			GRHIModule = std::make_shared<D3D11DynamicRHIModule>();
			if (GRHIModule->IsSupported())
				return GRHIModule->CreateRHI();
			return {};
		}
		else if (apiType == RHIAPIType::E_D3D12)
		{
			GRHIModule = std::make_shared<D3D12DynamicRHIModule>();
			if (GRHIModule->IsSupported())
				return GRHIModule->CreateRHI();
			return {};
		}
		else
		{
			return {};
		}
	}


	std::shared_ptr<DynamicRHI> GetDynamicRHI()
	{
		if (!GRHIModule)
		{
			return {};
		}
		return GRHIModule->CreateRHI();
	}

	void ReleasePlatformModule()
	{
		GRHIModule = {};
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