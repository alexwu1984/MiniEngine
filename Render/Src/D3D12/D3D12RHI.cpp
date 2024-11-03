#include "D3D12/D3D12RHI.h"
#include "RHIPrivate/D3D12RHIPrivate.h"
#include "D3D12/D3D12WindowDevice.h"
#include "D3D12/D3D12Adapter.h"

namespace RenderCore
{
	/**
 * Attempts to create a D3D12 device for the adapter using at minimum MinFeatureLevel.
 * If creation is successful, true is returned and the max supported feature level is set in OutMaxFeatureLevel.
 */
	static bool SafeTestD3D12CreateDevice(IDXGIAdapter* Adapter, D3D_FEATURE_LEVEL MinFeatureLevel, D3D_FEATURE_LEVEL& OutMaxFeatureLevel, uint32_t& OutNumDeviceNodes)
	{
		const D3D_FEATURE_LEVEL FeatureLevels[] =
		{
			// Add new feature levels that the app supports here.
			D3D_FEATURE_LEVEL_12_1,
			D3D_FEATURE_LEVEL_12_0,
			D3D_FEATURE_LEVEL_11_1,
			D3D_FEATURE_LEVEL_11_0
		};

		__try
		{
			ID3D12Device* pDevice = nullptr;
			if (SUCCEEDED(D3D12CreateDevice(Adapter, MinFeatureLevel, IID_PPV_ARGS(&pDevice))))
			{
				// Determine the max feature level supported by the driver and hardware.
				D3D_FEATURE_LEVEL MaxFeatureLevel = MinFeatureLevel;
				D3D12_FEATURE_DATA_FEATURE_LEVELS FeatureLevelCaps = {};
				FeatureLevelCaps.pFeatureLevelsRequested = FeatureLevels;
				FeatureLevelCaps.NumFeatureLevels = _ARRAYSIZE(FeatureLevels);
				if (SUCCEEDED(pDevice->CheckFeatureSupport(D3D12_FEATURE_FEATURE_LEVELS, &FeatureLevelCaps, sizeof(FeatureLevelCaps))))
				{
					MaxFeatureLevel = FeatureLevelCaps.MaxSupportedFeatureLevel;
				}

				OutMaxFeatureLevel = MaxFeatureLevel;
				OutNumDeviceNodes = pDevice->GetNodeCount();

				pDevice->Release();
				return true;
			}
		}
		__except (IsDelayLoadException(GetExceptionInformation()))
		{
			// We suppress warning C6322: Empty _except block. Appropriate checks are made upon returning. 
			//CA_SUPPRESS(6322);
		}

		return false;
	}

	bool D3D12DynamicRHIModule::IsSupported()
	{
		return false;
	}

	std::shared_ptr<DynamicRHI> D3D12DynamicRHIModule::CreateRHI()
	{
		return {};
	}

	static uint32_t CountAdapterOutputs(win32::com_ptr<IDXGIAdapter>& Adapter)
	{
		uint32_t OutputCount = 0;
		for (;;)
		{
			win32::com_ptr<IDXGIOutput> Output;
			HRESULT hr = Adapter->EnumOutputs(OutputCount, Output.get_init_ref());
			if (FAILED(hr))
			{
				break;
			}
			++OutputCount;
		}
		return OutputCount;
	}

	void D3D12DynamicRHIModule::FindAdapter()
	{
		assert(ChosenAdapters.size() == 0);

		// Try to create the DXGIFactory.  This will fail if we're not running Vista.
		win32::com_ptr<IDXGIFactory4> DXGIFactory;
		SafeCreateDXGIFactory(DXGIFactory.getpp());
		if (!DXGIFactory)
		{
			return;
		}

		bool bAllowPerfHUD = false;

		uint64_t HmdGraphicsAdapterLuid = 0;
		int32_t CVarExplicitAdapterValue = -2;
		const bool bFavorNonIntegrated = CVarExplicitAdapterValue == -1;

		win32::com_ptr<IDXGIAdapter> TempAdapter;
		const D3D_FEATURE_LEVEL MinRequiredFeatureLevel = GetRequiredD3DFeatureLevel();

		FD3D12AdapterDesc FirstWithoutIntegratedAdapter;
		FD3D12AdapterDesc FirstAdapter;

		bool bIsAnyAMD = false;
		bool bIsAnyIntel = false;
		bool bIsAnyNVIDIA = false;
		bool bRequestedWARP = D3D12RHI_ShouldCreateWithWarp();

		int PreferredVendor = D3D12RHI_PreferAdapterVendor();
		// Enumerate the DXGIFactory's adapters.
		for (uint32_t AdapterIndex = 0; DXGIFactory->EnumAdapters(AdapterIndex, TempAdapter.get_init_ref()) != DXGI_ERROR_NOT_FOUND; ++AdapterIndex)
		{
			// Check that if adapter supports D3D12.
			if (TempAdapter)
			{
				D3D_FEATURE_LEVEL MaxSupportedFeatureLevel = static_cast<D3D_FEATURE_LEVEL>(0);
				uint32_t NumNodes = 0;
				if (SafeTestD3D12CreateDevice(TempAdapter.get(), MinRequiredFeatureLevel, MaxSupportedFeatureLevel, NumNodes))
				{
					assert(NumNodes > 0);
					// Log some information about the available D3D12 adapters.
					DXGI_ADAPTER_DESC AdapterDesc;
					VERIFYD3DRESULT(TempAdapter->GetDesc(&AdapterDesc));
					uint32_t OutputCount = CountAdapterOutputs(TempAdapter);

					//UE_LOG(LogD3D12RHI, Log,
					//	TEXT("Found D3D12 adapter %u: %s (Max supported Feature Level %s)"),
					//	AdapterIndex,
					//	AdapterDesc.Description,
					//	GetFeatureLevelString(MaxSupportedFeatureLevel)
					//	);
					//UE_LOG(LogD3D12RHI, Log,
					//	TEXT("Adapter has %uMB of dedicated video memory, %uMB of dedicated system memory, and %uMB of shared system memory, %d output[s]"),
					//	(uint32)(AdapterDesc.DedicatedVideoMemory / (1024*1024)),
					//	(uint32)(AdapterDesc.DedicatedSystemMemory / (1024*1024)),
					//	(uint32)(AdapterDesc.SharedSystemMemory / (1024*1024)),
					//	OutputCount
					//	);


					bool bIsAMD = AdapterDesc.VendorId == 0x1002;
					bool bIsIntel = AdapterDesc.VendorId == 0x8086;
					bool bIsNVIDIA = AdapterDesc.VendorId == 0x10DE;
					bool bIsWARP = AdapterDesc.VendorId == 0x1414;

					if (bIsAMD) bIsAnyAMD = true;
					if (bIsIntel) bIsAnyIntel = true;
					if (bIsNVIDIA) bIsAnyNVIDIA = true;

					// Simple heuristic but without profiling it's hard to do better
					const bool bIsIntegrated = bIsIntel;
					// PerfHUD is for performance profiling
					//const bool bIsPerfHUD = !FCString::Stricmp(AdapterDesc.Description, TEXT("NVIDIA PerfHUD"));
					const bool bIsPerfHUD = false;
					FD3D12AdapterDesc CurrentAdapter(AdapterDesc, AdapterIndex, MaxSupportedFeatureLevel, NumNodes);

					// Requested WARP, reject all other adapters.
					const bool bSkipRequestedWARP = bRequestedWARP && !bIsWARP;

					// we don't allow the PerfHUD adapter
					const bool bSkipPerfHUDAdapter = bIsPerfHUD && !bAllowPerfHUD;

					// the HMD wants a specific adapter, not this one
					const bool bSkipHmdGraphicsAdapter = HmdGraphicsAdapterLuid != 0 && std::memcpy(&HmdGraphicsAdapterLuid, &AdapterDesc.AdapterLuid, sizeof(LUID)) != 0;

					// the user wants a specific adapter, not this one
					const bool bSkipExplicitAdapter = CVarExplicitAdapterValue >= 0 && AdapterIndex != CVarExplicitAdapterValue;

					const bool bSkipAdapter = bSkipRequestedWARP || bSkipPerfHUDAdapter || bSkipHmdGraphicsAdapter || bSkipExplicitAdapter;

					if (!bSkipAdapter)
					{
						if (!bIsIntegrated && !FirstWithoutIntegratedAdapter.IsValid())
						{
							FirstWithoutIntegratedAdapter = CurrentAdapter;
						}
						else if (PreferredVendor == AdapterDesc.VendorId && FirstWithoutIntegratedAdapter.IsValid())
						{
							FirstWithoutIntegratedAdapter = CurrentAdapter;
						}

						if (!FirstAdapter.IsValid())
						{
							FirstAdapter = CurrentAdapter;
						}
						else if (PreferredVendor == AdapterDesc.VendorId && FirstAdapter.IsValid())
						{
							FirstAdapter = CurrentAdapter;
						}
					}
				}
			}
		}

		std::shared_ptr<FD3D12Adapter> NewAdapter;
		if (bFavorNonIntegrated && (bIsAnyAMD || bIsAnyNVIDIA))
		{
			// We assume Intel is integrated graphics (slower than discrete) than NVIDIA or AMD cards and rather take a different one
			if (FirstWithoutIntegratedAdapter.IsValid())
			{
				NewAdapter = std::make_shared<FD3D12Adapter>(FD3D12Adapter(FirstWithoutIntegratedAdapter));
				ChosenAdapters.push_back(NewAdapter);
			}
			else
			{
				NewAdapter = std::make_shared<FD3D12Adapter>(FD3D12Adapter(FirstAdapter));
				ChosenAdapters.push_back(NewAdapter);
			}
		}
		else
		{
			NewAdapter = std::make_shared<FD3D12Adapter>(FD3D12Adapter(FirstAdapter));
			ChosenAdapters.push_back(NewAdapter);
		}
	}

	D3D12DynamicRHI::D3D12DynamicRHI()
	{

	}

	D3D12DynamicRHI::~D3D12DynamicRHI()
	{

	}

	void D3D12DynamicRHI::Init()
	{

	}

	void D3D12DynamicRHI::Shutdown()
	{

	}

	std::shared_ptr<RHICommandContext> D3D12DynamicRHI::GetDefaultCommandContext()
	{
		return {};
	}

	win32::com_ptr<ID3D12CommandQueue> D3D12DynamicRHI::CreateCommandQueue(FD3D12Device* Device, const D3D12_COMMAND_QUEUE_DESC& Desc)
	{
		win32::com_ptr<ID3D12CommandQueue> pCommandQueue;
		VERIFYD3DRESULT(Device->GetDevice()->CreateCommandQueue(&Desc, IID_PPV_ARGS(pCommandQueue.get_init_ref())));
		return pCommandQueue;
	}

}