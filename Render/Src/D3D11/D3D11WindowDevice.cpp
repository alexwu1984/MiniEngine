#include "D3D11/D3D11RHI.h"
#include "core/commandline.h"
#include "RHIPrivate/D3D11RHIPrivate.h"
#include "RHIPrivate/D3D11StateCachePrivate.h"
#include "core/strings.h"
#include "core/logger.h"
#include "win/platform_memory.h"
#include "Imgui/imgui_impl_dx11.h"

namespace RenderCore
{
	bool D3D11RHI_ShouldCreateWithD3DDebug()
	{
		return core::CommandLine::Get().GetSwitch("d3ddebug") ||
			core::CommandLine::Get().GetSwitch("dxdebug");

	}

	bool D3D11RHI_ShouldAllowAsyncResourceCreation()
	{
		static bool bAllowAsyncResourceCreation = !core::CommandLine::Get().GetName("nod3dasync");
		return bAllowAsyncResourceCreation;
	}

	/**
 * Returns the lowest D3D feature level we are allowed to created based on
 * command line parameters.
 */
	static D3D_FEATURE_LEVEL GetMinAllowedD3DFeatureLevel()
	{
		// Default to 11.0
		D3D_FEATURE_LEVEL AllowedFeatureLevel = D3D_FEATURE_LEVEL_11_0;
		return AllowedFeatureLevel;
	}

	/**
	 * Returns the highest D3D feature level we are allowed to created based on
	 * command line parameters.
	 */
	static D3D_FEATURE_LEVEL GetMaxAllowedD3DFeatureLevel()
	{
		// Default to 11.0
		D3D_FEATURE_LEVEL AllowedFeatureLevel = D3D_FEATURE_LEVEL_11_0;
		return AllowedFeatureLevel;
	}

	const TCHAR* GetFeatureLevelString(D3D_FEATURE_LEVEL FeatureLevel)
	{
		switch (FeatureLevel)
		{
		case D3D_FEATURE_LEVEL_9_1:		return TEXT("9_1");
		case D3D_FEATURE_LEVEL_9_2:		return TEXT("9_2");
		case D3D_FEATURE_LEVEL_9_3:		return TEXT("9_3");
		case D3D_FEATURE_LEVEL_10_0:	return TEXT("10_0");
		case D3D_FEATURE_LEVEL_10_1:	return TEXT("10_1");
		case D3D_FEATURE_LEVEL_11_0:	return TEXT("11_0");
		case D3D_FEATURE_LEVEL_11_1:	return TEXT("11_1");
		}
		return TEXT("X_X");
	}

	static uint32_t CountAdapterOutputs(win32::com_ptr<IDXGIAdapter>& Adapter)
	{
		uint32_t OutputCount = 0;
		for (;;)
		{
			win32::com_ptr<IDXGIOutput> Output;
			HRESULT hr = Adapter->EnumOutputs(OutputCount, Output.getpp());
			if (FAILED(hr))
			{
				break;
			}
			++OutputCount;
		}

		return OutputCount;
	}

	/** This function is used as a SEH filter to catch only delay load exceptions. */
	static bool IsDelayLoadException(PEXCEPTION_POINTERS ExceptionPointers)
	{
#if WINVER > 0x502	// Windows SDK 7.1 doesn't define VcppException
		switch (ExceptionPointers->ExceptionRecord->ExceptionCode)
		{
		case VcppException(ERROR_SEVERITY_ERROR, ERROR_MOD_NOT_FOUND):
		case VcppException(ERROR_SEVERITY_ERROR, ERROR_PROC_NOT_FOUND):
			return EXCEPTION_EXECUTE_HANDLER;
		default:
			return EXCEPTION_CONTINUE_SEARCH;
		}
#else
		return EXCEPTION_EXECUTE_HANDLER;
#endif
	}

	/**
	 * Since CreateDXGIFactory1 is a delay loaded import from the D3D11 DLL, if the user
	 * doesn't have VistaSP2/DX10, calling CreateDXGIFactory1 will throw an exception.
	 * We use SEH to detect that case and fail gracefully.
	 */
	static void SafeCreateDXGIFactory(IDXGIFactory1** DXGIFactory1)
	{
		__try
		{

			CreateDXGIFactory1(__uuidof(IDXGIFactory1), (void**)DXGIFactory1);
		}
		__except (IsDelayLoadException(GetExceptionInformation()))
		{
			Assert(false);
		}
	}

	/**
	 * Attempts to create a D3D11 device for the adapter using at most MaxFeatureLevel.
	 * If creation is successful, true is returned and the supported feature level is set in OutFeatureLevel.
	 */
	static bool SafeTestD3D11CreateDevice(IDXGIAdapter* Adapter, D3D_FEATURE_LEVEL MinFeatureLevel, D3D_FEATURE_LEVEL MaxFeatureLevel, D3D_FEATURE_LEVEL* OutFeatureLevel)
	{
		ID3D11Device* D3DDevice = nullptr;
		ID3D11DeviceContext* D3DDeviceContext = nullptr;
		uint32_t DeviceFlags = D3D11_CREATE_DEVICE_SINGLETHREADED;
		// Use a debug device if specified on the command line.
		if (D3D11RHI_ShouldCreateWithD3DDebug())
		{
			DeviceFlags |= D3D11_CREATE_DEVICE_DEBUG;
		}

		// @MIXEDREALITY_CHANGE : BEGIN - Add BGRA flag for Windows Mixed Reality HMD's
		DeviceFlags |= D3D11_CREATE_DEVICE_BGRA_SUPPORT;
		// @MIXEDREALITY_CHANGE : END

		D3D_FEATURE_LEVEL RequestedFeatureLevels[] =
		{
			D3D_FEATURE_LEVEL_11_1,
			D3D_FEATURE_LEVEL_11_0,
		};

		// Trim to allowed feature levels
		int32_t FirstAllowedFeatureLevel = 0;
		int32_t NumAllowedFeatureLevels = UE_ARRAY_COUNT(RequestedFeatureLevels);
		int32_t LastAllowedFeatureLevel = NumAllowedFeatureLevels - 1;

		while (FirstAllowedFeatureLevel < NumAllowedFeatureLevels)
		{
			if (RequestedFeatureLevels[FirstAllowedFeatureLevel] == MaxFeatureLevel)
			{
				break;
			}
			FirstAllowedFeatureLevel++;
		}

		while (LastAllowedFeatureLevel > 0)
		{
			if (RequestedFeatureLevels[LastAllowedFeatureLevel] >= MinFeatureLevel)
			{
				break;
			}
			LastAllowedFeatureLevel--;
		}

		NumAllowedFeatureLevels = LastAllowedFeatureLevel - FirstAllowedFeatureLevel + 1;
		if (MaxFeatureLevel < MinFeatureLevel || NumAllowedFeatureLevels <= 0)
		{
			return false;
		}

		__try
		{
			// We don't want software renderer. Ideally we specify D3D_DRIVER_TYPE_HARDWARE on creation but
			// when we specify an adapter we need to specify D3D_DRIVER_TYPE_UNKNOWN (otherwise the call fails).
			// We cannot check the device type later (seems this is missing functionality in D3D).
			HRESULT Result = D3D11CreateDevice(
				Adapter,
				D3D_DRIVER_TYPE_UNKNOWN,
				nullptr,
				DeviceFlags,
				&RequestedFeatureLevels[FirstAllowedFeatureLevel],
				NumAllowedFeatureLevels,
				D3D11_SDK_VERSION,
				&D3DDevice,
				OutFeatureLevel,
				&D3DDeviceContext);

			if (SUCCEEDED(Result))
			{
				D3DDevice->Release();
				D3DDeviceContext->Release();
				return true;
			}

		}
		__except (IsDelayLoadException(GetExceptionInformation()))
		{
			// We suppress warning C6322: Empty _except block. Appropriate checks are made upon returning. 
			Assert(false);
		}

		return false;
	}


	bool D3D11DynamicRHI::FindAdapter()
	{
		C_P(D3D11DynamicRHI);
		if (!d->DXGIFactory1)
			SafeCreateDXGIFactory(&d->DXGIFactory1);
		win32::com_ptr<IDXGIFactory6> DXGIFactory6;
		d->DXGIFactory1->QueryInterface(__uuidof(IDXGIFactory6), DXGIFactory6.getvv());

		win32::com_ptr<IDXGIAdapter> TempAdapter;
		D3D_FEATURE_LEVEL MinAllowedFeatureLevel = GetMinAllowedD3DFeatureLevel();
		D3D_FEATURE_LEVEL MaxAllowedFeatureLevel = GetMaxAllowedD3DFeatureLevel();

		core::inf() << "D3D11 min allowed feature level: " << GetFeatureLevelString(MinAllowedFeatureLevel);
		core::inf() << "D3D11 max allowed feature level: " << GetFeatureLevelString(MaxAllowedFeatureLevel);


		D3D11Adapter FirstWithoutIntegratedAdapter;
		D3D11Adapter FirstAdapter;
		// indexed by AdapterIndex, we store it instead of query it later from the created device to prevent some Optimus bug reporting the data/name of the wrong adapter
		std::vector<DXGI_ADAPTER_DESC> AdapterDescription;

		bool bIsAnyAMD = false;
		bool bIsAnyIntel = false;
		bool bIsAnyNVIDIA = false;
		
		core::inf() << "D3D11 adapters:";

		int GpuPreferenceInt = DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE;
		//FParse::Value(FCommandLine::Get(), TEXT("-gpupreference="), GpuPreferenceInt);
		DXGI_GPU_PREFERENCE GpuPreference;
		switch (GpuPreferenceInt)
		{
		case 1: GpuPreference = DXGI_GPU_PREFERENCE_MINIMUM_POWER; break;
		case 2: GpuPreference = DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE; break;
		default: GpuPreference = DXGI_GPU_PREFERENCE_UNSPECIFIED; break;
		}

		auto LocalEnumAdapters = [&DXGIFactory6, GpuPreference,this](UINT AdapterIndex, IDXGIAdapter** Adapter) -> HRESULT
		{
			C_P(D3D11DynamicRHI);
			if (!DXGIFactory6 || GpuPreference == DXGI_GPU_PREFERENCE_UNSPECIFIED)
			{
				return d->DXGIFactory1->EnumAdapters(AdapterIndex, Adapter);
			}
			else
			{
				return DXGIFactory6->EnumAdapterByGpuPreference(AdapterIndex, DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE, __uuidof(IDXGIAdapter), (void**)Adapter);
			}
		};

		// Enumerate the DXGIFactory's adapters.
		for (uint32_t AdapterIndex = 0; LocalEnumAdapters(AdapterIndex, TempAdapter.get_init_ref()) != DXGI_ERROR_NOT_FOUND; ++AdapterIndex)
		{
			// to make sure the array elements can be indexed with AdapterIndex
			AdapterDescription.push_back({});
			DXGI_ADAPTER_DESC& AdapterDesc = AdapterDescription[AdapterDescription.size() - 1];
			//DXGI_ADAPTER_DESC& AdapterDesc = AdapterDescription[AdapterDescription.AddZeroed()];

			// Check that if adapter supports D3D11.
			if (TempAdapter)
			{
				D3D_FEATURE_LEVEL ActualFeatureLevel = (D3D_FEATURE_LEVEL)0;
				if (SafeTestD3D11CreateDevice(TempAdapter.get(), MinAllowedFeatureLevel, MaxAllowedFeatureLevel, &ActualFeatureLevel))
				{
					// Log some information about the available D3D11 adapters.
					VERIFYD3DRESULT(TempAdapter->GetDesc(&AdapterDesc));
					uint32_t OutputCount = CountAdapterOutputs(TempAdapter);

					core::LOG(core::log_inf,
						WIDE2("  %2u. '%s' (Feature Level %s)"),
						AdapterIndex,
						AdapterDesc.Description,
						GetFeatureLevelString(ActualFeatureLevel)
					);
					core::LOG(core::log_inf,
						WIDE2("      %u/%u/%u MB DedicatedVideo/DedicatedSystem/SharedSystem, Outputs:%d, VendorId:0x%x"),
						(uint32_t)(AdapterDesc.DedicatedVideoMemory / (1024 * 1024)),
						(uint32_t)(AdapterDesc.DedicatedSystemMemory / (1024 * 1024)),
						(uint32_t)(AdapterDesc.SharedSystemMemory / (1024 * 1024)),
						OutputCount,
						AdapterDesc.VendorId
					);

					bool bIsAMD = AdapterDesc.VendorId == 0x1002;
					bool bIsIntel = AdapterDesc.VendorId == 0x8086;
					bool bIsNVIDIA = AdapterDesc.VendorId == 0x10DE;
					bool bIsMicrosoft = AdapterDesc.VendorId == 0x1414;

					if (bIsAMD) bIsAnyAMD = true;
					if (bIsIntel) bIsAnyIntel = true;
					if (bIsNVIDIA) bIsAnyNVIDIA = true;

					// Simple heuristic but without profiling it's hard to do better
					bool bIsNonLocalMemoryPresent = false;
					if (bIsIntel)
					{
						win32::com_ptr<IDXGIAdapter3> TempDxgiAdapter3;
						DXGI_QUERY_VIDEO_MEMORY_INFO NonLocalVideoMemoryInfo;
						if (SUCCEEDED(TempAdapter->QueryInterface(__uuidof(IDXGIAdapter3), (void**)TempDxgiAdapter3.get_init_ref())) &&
							TempDxgiAdapter3.is_valid() && SUCCEEDED(TempDxgiAdapter3->QueryVideoMemoryInfo(0, DXGI_MEMORY_SEGMENT_GROUP_NON_LOCAL, &NonLocalVideoMemoryInfo)))
						{
							bIsNonLocalMemoryPresent = NonLocalVideoMemoryInfo.Budget != 0;
						}
					}
					const bool bIsIntegrated = bIsIntel && !bIsNonLocalMemoryPresent;

					D3D11Adapter CurrentAdapter(AdapterIndex, ActualFeatureLevel);

					if (!bIsIntegrated && !FirstWithoutIntegratedAdapter.IsValid())
					{
						FirstWithoutIntegratedAdapter = CurrentAdapter;
					}


					if (!FirstAdapter.IsValid())
					{
						FirstAdapter = CurrentAdapter;
					}

				}
			}
		}

		d->ChosenAdapter = FirstWithoutIntegratedAdapter;

		// We assume Intel is integrated graphics (slower than discrete) than NVIDIA or AMD cards and rather take a different one
		if (!d->ChosenAdapter.IsValid())
		{
			d->ChosenAdapter = FirstAdapter;
		}

		if (d->ChosenAdapter.IsValid())
		{
			d->ChosenDescription = AdapterDescription[d->ChosenAdapter.AdapterIndex];
			core::LOG(core::log_inf,TEXT("Chosen D3D11 Adapter: %u"), d->ChosenAdapter.AdapterIndex);
		}
		else
		{
			core::LOG(core::log_err, TEXT("Failed to choose a D3D11 Adapter."));
		}

		// The hardware must support at least 10.0 (usually 11_0, 10_0 or 10_1).
		return d->ChosenAdapter.IsValid()
			&& d->ChosenAdapter.MaxSupportedFeatureLevel != D3D_FEATURE_LEVEL_9_1
			&& d->ChosenAdapter.MaxSupportedFeatureLevel != D3D_FEATURE_LEVEL_9_2
			&& d->ChosenAdapter.MaxSupportedFeatureLevel != D3D_FEATURE_LEVEL_9_3;
	}

	bool D3D11DynamicRHI::InitD3DDevice()
	{
		C_P(D3D11DynamicRHI);
		if(!d->DXGIFactory1)
			SafeCreateDXGIFactory(d->DXGIFactory1.getpp());

		if (!d->ChosenAdapter.IsValid())
		{
			if (!FindAdapter())
			{
				return false;
			}
		}

		// Determine the adapter and device type to use.
		win32::com_ptr<IDXGIAdapter> Adapter;

		// In Direct3D 11, if you are trying to create a hardware or a software device, set pAdapter != NULL which constrains the other inputs to be:
		//		DriverType must be D3D_DRIVER_TYPE_UNKNOWN 
		//		Software must be NULL. 
		D3D_DRIVER_TYPE DriverType = D3D_DRIVER_TYPE_UNKNOWN;

		uint32_t DeviceFlags = D3D11RHI_ShouldAllowAsyncResourceCreation() ? 0 : D3D11_CREATE_DEVICE_SINGLETHREADED;

		// Use a debug device if specified on the command line.
		const bool bWithD3DDebug = D3D11RHI_ShouldCreateWithD3DDebug();

		if (bWithD3DDebug)
		{
			DeviceFlags |= D3D11_CREATE_DEVICE_DEBUG;

			core::inf() << core::format("InitD3DDevice: -D3DDebug = ", bWithD3DDebug ? "on" : "off");

		}


		// @MIXEDREALITY_CHANGE : BEGIN - Add BGRA flag for Windows Mixed Reality HMD's
		DeviceFlags |= D3D11_CREATE_DEVICE_BGRA_SUPPORT;
		// @MIXEDREALITY_CHANGE : END

		GTexturePoolSize = 0;

		win32::com_ptr<IDXGIAdapter> EnumAdapter;

		if (d->DXGIFactory1->EnumAdapters(d->ChosenAdapter.AdapterIndex, EnumAdapter.get_init_ref()) != DXGI_ERROR_NOT_FOUND)
		{
			// we don't use AdapterDesc.Description as there is a bug with Optimus where it can report the wrong name
			DXGI_ADAPTER_DESC AdapterDesc = d->ChosenDescription;
			Adapter = EnumAdapter;

			GRHIAdapterName = AdapterDesc.Description;
			GRHIVendorId = AdapterDesc.VendorId;
			GRHIDeviceId = AdapterDesc.DeviceId;
			GRHIDeviceRevision = AdapterDesc.Revision;

			core::LOG(core::log_inf, TEXT("    GPU DeviceId: 0x%x (for the marketing name, search the web for \"GPU Device Id\")"),
				AdapterDesc.DeviceId);

			// Issue: 32bit windows doesn't report 64bit value, we take what we get.
			D3D11GlobalStats::GDedicatedVideoMemory = int64_t(AdapterDesc.DedicatedVideoMemory);
			D3D11GlobalStats::GDedicatedSystemMemory = int64_t(AdapterDesc.DedicatedSystemMemory);
			D3D11GlobalStats::GSharedSystemMemory = int64_t(AdapterDesc.SharedSystemMemory);

			// Total amount of system memory, clamped to 8 GB
			int64_t TotalPhysicalMemory = std::min(int64_t(win32::Win32MemoryConstants::GetConstants().TotalPhysicalGB), 8ll) * (1024ll * 1024ll * 1024ll);

			// Consider 50% of the shared memory but max 25% of total system memory.
			int64_t ConsideredSharedSystemMemory = std::min(D3D11GlobalStats::GSharedSystemMemory / 2ll, TotalPhysicalMemory / 4ll);

			win32::com_ptr<IDXGIAdapter3> DxgiAdapter3;
			DXGI_QUERY_VIDEO_MEMORY_INFO LocalVideoMemoryInfo;
			D3D11GlobalStats::GTotalGraphicsMemory = 0;
			if (IsRHIDeviceIntel())
			{
				// It's all system memory.
				D3D11GlobalStats::GTotalGraphicsMemory = D3D11GlobalStats::GDedicatedVideoMemory;
				D3D11GlobalStats::GTotalGraphicsMemory += D3D11GlobalStats::GDedicatedSystemMemory;
				D3D11GlobalStats::GTotalGraphicsMemory += ConsideredSharedSystemMemory;
			}
			else if (IsRHIDeviceAMD() && SUCCEEDED(Adapter->QueryInterface(__uuidof(IDXGIAdapter3), (void**)DxgiAdapter3.get_init_ref())) &&
				DxgiAdapter3.is_valid() && SUCCEEDED(DxgiAdapter3->QueryVideoMemoryInfo(0, DXGI_MEMORY_SEGMENT_GROUP_LOCAL, &LocalVideoMemoryInfo)))
			{
				// use the entire budget for D3D11, in keeping with setting GTotalGraphicsMemory to all of AdapterDesc.DedicatedVideoMemory
				// in the other method directly below
				D3D11GlobalStats::GTotalGraphicsMemory = LocalVideoMemoryInfo.Budget;
			}
			else if (D3D11GlobalStats::GDedicatedVideoMemory >= 200 * 1024 * 1024)
			{
				// Use dedicated video memory, if it's more than 200 MB
				D3D11GlobalStats::GTotalGraphicsMemory = D3D11GlobalStats::GDedicatedVideoMemory;
			}
			else if (D3D11GlobalStats::GDedicatedSystemMemory >= 200 * 1024 * 1024)
			{
				// Use dedicated system memory, if it's more than 200 MB
				D3D11GlobalStats::GTotalGraphicsMemory = D3D11GlobalStats::GDedicatedSystemMemory;
			}
			else if (D3D11GlobalStats::GSharedSystemMemory >= 400 * 1024 * 1024)
			{
				// Use some shared system memory, if it's more than 400 MB
				D3D11GlobalStats::GTotalGraphicsMemory = ConsideredSharedSystemMemory;
			}
			else
			{
				// Otherwise consider 25% of total system memory for graphics.
				D3D11GlobalStats::GTotalGraphicsMemory = TotalPhysicalMemory / 4ll;
			}

			if (sizeof(SIZE_T) < 8)
			{
				// Clamp to 1 GB if we're less than 64-bit
				D3D11GlobalStats::GTotalGraphicsMemory = math::Min(D3D11GlobalStats::GTotalGraphicsMemory, 1024ll * 1024ll * 1024ll);
			}

		}
		
		bool bDeviceCreated = false;
		D3D_FEATURE_LEVEL ActualFeatureLevel = (D3D_FEATURE_LEVEL)0;

		if (!bDeviceCreated)
		{
			// Creating the Direct3D device.
			VERIFYD3DRESULT(D3D11CreateDevice(
				Adapter.get(),
				DriverType,
				NULL,
				DeviceFlags,
				&d->FeatureLevel,
				1,
				D3D11_SDK_VERSION,
				d->Direct3DDevice.get_init_ref(),
				&ActualFeatureLevel,
				d->Direct3DDeviceIMContext.get_init_ref()
			));
		}
		d->Direct3DDevice2 = win32::com_ptr<ID3D11Device2>(d->Direct3DDevice);
		d->Direct3DDeviceIMContext2 = win32::com_ptr<ID3D11DeviceContext2>(d->Direct3DDeviceIMContext);

		d->StateCache.Init(d->Direct3DDeviceIMContext.get());

		::ImGui_ImplDX11_Init(d->Direct3DDevice.get(), d->Direct3DDeviceIMContext.get());

		IUnknown* RenderDoc;
		IID RenderDocID;

		bool bRenderDoc = false;

		if (SUCCEEDED(IIDFromString(L"{A7AA6116-9C8D-4BBA-9083-B4D816B71B78}", &RenderDocID)))
		{
			if (SUCCEEDED(d->Direct3DDevice->QueryInterface(RenderDocID, (void**)(&RenderDoc))))
			{
				bRenderDoc = true;
				core::inf() << " Running under renderdoc";
			}
		}

		IUnknown* IntelGPA;
		static const IID IntelGPAID = { 0xCCFFEF16, 0x7B69, 0x468F, {0xBC, 0xE3, 0xCD, 0x95, 0x33, 0x69, 0xA3, 0x9A} };

		if (SUCCEEDED(d->Direct3DDevice->QueryInterface(IntelGPAID, (void**)(&IntelGPA))))
		{
			// Running under Intel GPA, so enable capturing mode
			core::inf() << " Running under Intel GPA, so enable capturing mode";
		}

		if (DeviceFlags & D3D11_CREATE_DEVICE_DEBUG)
		{
			win32::com_ptr<ID3D11InfoQueue> InfoQueue;
			VERIFYD3DRESULT(d->Direct3DDevice->QueryInterface(IID_ID3D11InfoQueue, (void**)InfoQueue.get_init_ref()));
			if (InfoQueue)
			{
				D3D11_INFO_QUEUE_FILTER NewFilter;
				ZeroMemory(&NewFilter, sizeof(NewFilter));

				// Turn off info msgs as these get really spewy
				D3D11_MESSAGE_SEVERITY DenySeverity = D3D11_MESSAGE_SEVERITY_INFO;
				NewFilter.DenyList.NumSeverities = 1;
				NewFilter.DenyList.pSeverityList = &DenySeverity;

				// Be sure to carefully comment the reason for any additions here!  Someone should be able to look at it later and get an idea of whether it is still necessary.
				D3D11_MESSAGE_ID DenyIds[] = {
					// OMSETRENDERTARGETS_INVALIDVIEW - d3d will complain if depth and color targets don't have the exact same dimensions, but actually
					//	if the color target is smaller then things are ok.  So turn off this error.  There is a manual check in FD3D11DynamicRHI::SetRenderTarget
					//	that tests for depth smaller than color and MSAA settings to match.
					D3D11_MESSAGE_ID_OMSETRENDERTARGETS_INVALIDVIEW,

					// QUERY_BEGIN_ABANDONING_PREVIOUS_RESULTS - The RHI exposes the interface to make and issue queries and a separate interface to use that data.
					//		Currently there is a situation where queries are issued and the results may be ignored on purpose.  Filtering out this message so it doesn't
					//		swarm the debug spew and mask other important warnings
					D3D11_MESSAGE_ID_QUERY_BEGIN_ABANDONING_PREVIOUS_RESULTS,
					D3D11_MESSAGE_ID_QUERY_END_ABANDONING_PREVIOUS_RESULTS,

					// D3D11_MESSAGE_ID_CREATEINPUTLAYOUT_EMPTY_LAYOUT - This is a warning that gets triggered if you use a null vertex declaration,
					//       which we want to do when the vertex shader is generating vertices based on ID.
					D3D11_MESSAGE_ID_CREATEINPUTLAYOUT_EMPTY_LAYOUT,

					// D3D11_MESSAGE_ID_DEVICE_DRAW_INDEX_BUFFER_TOO_SMALL - This warning gets triggered by Slate draws which are actually using a valid index range.
					//		The invalid warning seems to only happen when VS 2012 is installed.  Reported to MS.  
					//		There is now an assert in DrawIndexedPrimitive to catch any valid errors reading from the index buffer outside of range.
					D3D11_MESSAGE_ID_DEVICE_DRAW_INDEX_BUFFER_TOO_SMALL,

					// D3D11_MESSAGE_ID_DEVICE_DRAW_RENDERTARGETVIEW_NOT_SET - This warning gets triggered by shadow depth rendering because the shader outputs
					//		a color but we don't bind a color render target. That is safe as writes to unbound render targets are discarded.
					//		Also, batched elements triggers it when rendering outside of scene rendering as it outputs to deferred normals RT which is not bound.
					(D3D11_MESSAGE_ID)3146081, // D3D11_MESSAGE_ID_DEVICE_DRAW_RENDERTARGETVIEW_NOT_SET,

					// Spams constantly as we change the debug name on rendertargets that get reused.
					D3D11_MESSAGE_ID_SETPRIVATEDATA_CHANGINGPARAMS,
				};

				NewFilter.DenyList.NumIDs = sizeof(DenyIds) / sizeof(D3D11_MESSAGE_ID);
				NewFilter.DenyList.pIDList = (D3D11_MESSAGE_ID*)&DenyIds;

				InfoQueue->PushStorageFilter(&NewFilter);

				// Break on D3D debug errors.
				InfoQueue->SetBreakOnSeverity(D3D11_MESSAGE_SEVERITY_ERROR, true);

				// Enable this to break on a specific id in order to quickly get a callstack
				InfoQueue->SetBreakOnID(D3D11_MESSAGE_ID_DEVICE_DRAW_CONSTANT_BUFFER_TOO_SMALL, true);

				if (core::CommandLine::Get().GetName("d3dbreakonwarning"))
				{
					InfoQueue->SetBreakOnSeverity(D3D11_MESSAGE_SEVERITY_WARNING, true);
				}
			}
		}

		return true;
	}
}