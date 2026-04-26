#include "D3D12/D3D12Adapter.h"
#include "D3D12/D3D12RHI.h"
#include "core/commandline.h"
#include "core/logger.h"
#include "D3D12/D3D12DirectCommandListManager.h"
#include "D3D12/D3D12WindowDevice.h"
#include "D3D12/D3D12Allocation.h"
#include "D3D12/D3D12RootSignature.h"
#include "D3D12/D3D12DescriptorCache.h"
#include "D3D12/D3D12UploadWCDiagnostics.h"
#include "D3D12/D3D12CallStats.h"
#include "Imgui/imgui_impl_dx12.h"
#include <d3d12sdklayers.h>
#include <dxgidebug.h>

namespace RenderCore
{
	struct FD3D12AdapterPrivate
	{
		// LDA setups have one ID3D12Device
		std::weak_ptr<D3D12DynamicRHI> OwningRHI;
		win32::com_ptr<ID3D12Device> RootDevice;
		win32::com_ptr<ID3D12Device1> RootDevice1;
		win32::com_ptr<ID3D12Device2> RootDevice2;
		win32::com_ptr<IDXGIFactory> DxgiFactory;
		win32::com_ptr<IDXGIFactory2> DxgiFactory2;
		win32::com_ptr<IDXGIAdapter> DxgiAdapter;
		win32::com_ptr<IDXGIDebug> DxgiDebug;

		D3D12_RESOURCE_HEAP_TIER ResourceHeapTier;
		D3D12_RESOURCE_BINDING_TIER ResourceBindingTier;
		D3D_ROOT_SIGNATURE_VERSION RootSignatureVersion;

		std::shared_ptr<FD3D12FenceCorePool> FenceCorePool;
		std::shared_ptr<FD3D12ManualFence> FrameFence;

		FD3D12AdapterDesc Desc;
		bool bDeviceRemoved = false;
		bool bDepthBoundsTestSupported = false;

		std::shared_ptr<FD3D12Device> Device;
		std::shared_ptr<FRootSignature> RootSignature;

		std::shared_ptr<FDynamicDescriptorHeap> DynamicViewDescriptorHeap;

		~FD3D12AdapterPrivate()
		{
			// D3D12 object counts go to the Windows debug channel (VS Output -> Show output from: Debug),
			// not Engine.log. Must run while the device is still alive.
			if (RootDevice)
			{
				win32::com_ptr<ID3D12DebugDevice> DebugDevice;
				if (SUCCEEDED(RootDevice->QueryInterface(IID_PPV_ARGS(DebugDevice.get_init_ref()))))
				{
					core::inf() << "D3D12: ReportLiveDeviceObjects (see Visual Studio Output window, source: Debug)";
					DebugDevice->ReportLiveDeviceObjects(D3D12_RLDO_DETAIL);
				}
			}

			RootDevice = {};
			RootDevice1 = {};
			RootDevice2 = {};
			DxgiFactory = {};
			DxgiFactory2 = {};
			DxgiAdapter = {};
			RootSignature = {};
			FenceCorePool = {};
			FrameFence = {};
			Device = {};
			DynamicViewDescriptorHeap = {};

			if (DxgiDebug)
			{
				core::inf() << "D3D12: DXGI ReportLiveObjects (see Visual Studio Output window, source: Debug)";
				DxgiDebug->ReportLiveObjects(DXGI_DEBUG_ALL, DXGI_DEBUG_RLO_DETAIL);
			}
			DxgiDebug = {};
		}
		
	};

	FD3D12Adapter::FD3D12Adapter(const FD3D12AdapterDesc& desc)
		:d_ptr(new FD3D12AdapterPrivate())
	{
		C_P(FD3D12Adapter);
		d->Desc = desc;
	}

	FD3D12Adapter::~FD3D12Adapter()
	{
		delete d_ptr;
	}

	void FD3D12Adapter::Initialize(const std::weak_ptr<D3D12DynamicRHI>& RHI)
	{
		C_P(FD3D12Adapter);
		Assert(!RHI.expired());
		d->OwningRHI = RHI;
	}

	void FD3D12Adapter::InitializeDevices()
	{
		C_P(FD3D12Adapter);
		if (d->bDeviceRemoved)
		{
			Assert(d->RootDevice.is_valid());

			HRESULT hRes = d->RootDevice->GetDeviceRemovedReason();

			const TCHAR* Reason = TEXT("?");
			switch (hRes)
			{
			case DXGI_ERROR_DEVICE_HUNG:			Reason = TEXT("HUNG"); break;
			case DXGI_ERROR_DEVICE_REMOVED:			Reason = TEXT("REMOVED"); break;
			case DXGI_ERROR_DEVICE_RESET:			Reason = TEXT("RESET"); break;
			case DXGI_ERROR_DRIVER_INTERNAL_ERROR:	Reason = TEXT("INTERNAL_ERROR"); break;
			case DXGI_ERROR_INVALID_CALL:			Reason = TEXT("INVALID_CALL"); break;
			}

			d->bDeviceRemoved = false;

			Cleanup();

			// We currently don't support removed devices because FTexture2DResource can't recreate its RHI resources from scratch.
			// We would also need to recreate the viewport swap chains from scratch.
			//UE_LOG(LogD3D12RHI, Fatal, TEXT("The Direct3D 12 device that was being used has been removed (Error: %d '%s').  Please restart the game."), hRes, Reason);
			core::err() << core::formatw(L"The Direct3D 12 device that was being used has been removed (Error: ", hRes, L"'", Reason, "'). Please restart the game.)");

		}

		// Use a debug device if specified on the command line.
		bool bWithD3DDebug = D3D12RHI_ShouldCreateWithD3DDebug();

		// If we don't have a device yet, either because this is the first viewport, or the old device was removed, create a device.
		if (!d->RootDevice)
		{
			if (!CreateRootDevice(bWithD3DDebug))
				return;

			if (SUCCEEDED(d->RootDevice->QueryInterface(IID_PPV_ARGS(d->RootDevice1.get_init_ref()))))
			{
				//UE_LOG(LogD3D12RHI, Log, TEXT("The system supports ID3D12Device1."));
				core::inf() << "The system supports ID3D12Device1.";
			}

			if (SUCCEEDED(d->RootDevice->QueryInterface(IID_PPV_ARGS(d->RootDevice2.get_init_ref()))))
			{
				//UE_LOG(LogD3D12RHI, Log, TEXT("The system supports ID3D12Device2."));
				core::inf() << "The system supports ID3D12Device2.";
			}
		}

		D3D12_FEATURE_DATA_D3D12_OPTIONS D3D12Caps;
		ZeroMemory(&D3D12Caps, sizeof(D3D12Caps));
		HRESULT hr = d->RootDevice->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS, &D3D12Caps, sizeof(D3D12Caps));
		d->ResourceHeapTier = D3D12Caps.ResourceHeapTier;
		d->ResourceBindingTier = D3D12Caps.ResourceBindingTier;

		D3D12_FEATURE_DATA_D3D12_OPTIONS2 D3D12Caps2 = {};
		if (FAILED(d->RootDevice->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS2, &D3D12Caps2, sizeof(D3D12Caps2))))
		{
			D3D12Caps2.DepthBoundsTestSupported = false;
			D3D12Caps2.ProgrammableSamplePositionsTier = D3D12_PROGRAMMABLE_SAMPLE_POSITIONS_TIER_NOT_SUPPORTED;
		}
		d->bDepthBoundsTestSupported = !!D3D12Caps2.DepthBoundsTestSupported;

		D3D12_FEATURE_DATA_ROOT_SIGNATURE D3D12RootSignatureCaps = {};
		D3D12RootSignatureCaps.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_1;	// This is the highest version we currently support. If CheckFeatureSupport succeeds, the HighestVersion returned will not be greater than this.
		if (FAILED(d->RootDevice->CheckFeatureSupport(D3D12_FEATURE_ROOT_SIGNATURE, &D3D12RootSignatureCaps, sizeof(D3D12RootSignatureCaps))))
		{
			D3D12RootSignatureCaps.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_0;
		}
		d->RootSignatureVersion = D3D12RootSignatureCaps.HighestVersion;

		d->FrameFence = std::make_shared<FD3D12ManualFence>(this->shared_from_this(), L"Adapter Frame Fence");
		d->FrameFence->CreateFence();

		d->Device = std::make_shared<FD3D12Device>(this->shared_from_this());
		d->Device->Initialize();

		CreateSignatures();

		d->DynamicViewDescriptorHeap = std::make_shared<FDynamicDescriptorHeap>(d->Device,
																				d->Device->GetDefaultCommandContext(), 
																			    D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
		// Keep ImGui backend lifetime consistent with D3D12DynamicRHI::Shutdown():
		// when noimgui=1, we must not initialize ImGui_ImplDX12 (it allocates g_pFrameResources).
		if (!core::CommandLine::Get().GetName("noimgui"))
		{
			win32::com_ptr<ID3D12DescriptorHeap> DescriptorHeap = d->DynamicViewDescriptorHeap->GetHeapPointer();
			::ImGui_ImplDX12_Init(d->RootDevice.get(), WINDOWS_DEFAULT_NUM_BACK_BUFFERS,
				DXGI_FORMAT_R8G8B8A8_UNORM, DescriptorHeap.get(),
				DescriptorHeap->GetCPUDescriptorHandleForHeapStart(),
				DescriptorHeap->GetGPUDescriptorHandleForHeapStart());
		}
	}

	void FD3D12Adapter::InitializeRayTracing()
	{

	}

	void FD3D12Adapter::SetDeviceRemoved(bool value)
	{
		C_P(FD3D12Adapter);
		d->bDeviceRemoved = value;
	}

	bool FD3D12Adapter::IsDeviceRemoved() const
	{
		C_P(const FD3D12Adapter);
		return d->bDeviceRemoved;
	}

	ID3D12Device* FD3D12Adapter::GetD3DDevice() const
	{
		C_P(const FD3D12Adapter);
		return d->RootDevice.get();
	}

	ID3D12Device1* FD3D12Adapter::GetD3DDevice1() const
	{
		C_P(const FD3D12Adapter);
		return d->RootDevice1.get();
	}

	ID3D12Device2* FD3D12Adapter::GetD3DDevice2() const
	{
		C_P(const FD3D12Adapter);
		return d->RootDevice2.get();
	}

	FD3D12FenceCorePool& FD3D12Adapter::GetFenceCorePool()
	{
		C_P(FD3D12Adapter);
		if (!d->FenceCorePool)
		{
			d->FenceCorePool = std::make_shared<FD3D12FenceCorePool>(shared_from_this());
		}
		return *d->FenceCorePool;
	}

	FD3D12ManualFence& FD3D12Adapter::GetFrameFence()
	{
		C_P(FD3D12Adapter);
		Assert(d->FrameFence.get());
		return *d->FrameFence;
	}

	std::shared_ptr<FD3D12Device> FD3D12Adapter::GetDevice() const
	{
		C_P(const FD3D12Adapter);
		return d->Device;
	}

	std::shared_ptr<D3D12DynamicRHI> FD3D12Adapter::GetOwningRHI() const
	{
		C_P(FD3D12Adapter);
		Assert(!d->OwningRHI.expired());
		return d->OwningRHI.lock();
	}

	D3D12_RESOURCE_HEAP_TIER FD3D12Adapter::GetResourceHeapTier() const
	{
		C_P(const FD3D12Adapter);
		return d->ResourceHeapTier;
	}

	D3D12_RESOURCE_BINDING_TIER FD3D12Adapter::GetResourceBindingTier() const
	{
		C_P(const FD3D12Adapter);
		return d->ResourceBindingTier;
	}

	D3D_ROOT_SIGNATURE_VERSION FD3D12Adapter::GetRootSignatureVersion() const
	{
		C_P(const FD3D12Adapter);
		return d->RootSignatureVersion;
	}

	bool FD3D12Adapter::IsDepthBoundsTestSupported() const
	{
		C_P(const FD3D12Adapter);
		return d->bDepthBoundsTestSupported;
	}

	void FD3D12Adapter::CreateSignatures()
	{
		C_P(FD3D12Adapter);
		ID3D12Device* Device = GetD3DDevice();
		d->RootSignature = std::make_shared<FRootSignature>(d->Device, 0, 0);
	}

	uint32_t FD3D12Adapter::GetAdapterIndex() const
	{
		C_P(const FD3D12Adapter);
		return d->Desc.AdapterIndex;
	}

	D3D_FEATURE_LEVEL FD3D12Adapter::GetFeatureLevel() const
	{
		C_P(const FD3D12Adapter);
		return d->Desc.MaxSupportedFeatureLevel;
	}

	const DXGI_ADAPTER_DESC& FD3D12Adapter::GetD3DAdapterDesc() const
	{
		C_P(const FD3D12Adapter);
		return d->Desc.Desc;
	}

	IDXGIAdapter* FD3D12Adapter::GetAdapter()
	{
		C_P(FD3D12Adapter);
		return d->DxgiAdapter.get();
	}

	const FD3D12AdapterDesc& FD3D12Adapter::GetDesc() const
	{
		C_P(const FD3D12Adapter);
		return d->Desc;
	}

	IDXGIFactory* FD3D12Adapter::GetDXGIFactory() const
	{
		C_P(const FD3D12Adapter);
		return d->DxgiFactory.get();
	}

	IDXGIFactory2* FD3D12Adapter::GetDXGIFactory2() const
	{
		C_P(const FD3D12Adapter);
		return d->DxgiFactory2.get();
	}

	std::shared_ptr<FRootSignature> FD3D12Adapter::GetRootSignature() const
	{
		C_P(const FD3D12Adapter);
		return d->RootSignature;
	}


	HRESULT FD3D12Adapter::CreateCommittedResource(const D3D12_RESOURCE_DESC& InDesc, const D3D12_HEAP_PROPERTIES& HeapProps,
												  const D3D12_RESOURCE_STATES& InitialUsage, const D3D12_CLEAR_VALUE* ClearValue, 
												  FD3D12Resource** ppOutResource, const wchar_t* Name)
	{
		C_P(FD3D12Adapter);
		if (!ppOutResource)
		{
			return E_POINTER;
		}

		//LLM_PLATFORM_SCOPE(ELLMTag::GraphicsPlatform);

		// Diagnostics: large UPLOAD buffers are often the source of WC growth in D3D12.
		// Keep it lightweight and only for big buffers.
		if (HeapProps.Type == D3D12_HEAP_TYPE_UPLOAD && InDesc.Dimension == D3D12_RESOURCE_DIMENSION_BUFFER)
		{
			const uint64_t bytes = (uint64_t)InDesc.Width;
			if (bytes >= 16ull * 1024ull * 1024ull)
			{
				D3D12UploadWCDiagnostics_OnCreateUploadCommittedBuffer(Name ? Name : L"UploadBuffer", (std::size_t)bytes);
			}
		}

		win32::com_ptr<ID3D12Resource> pResource;
		Render::D3D12CallStats::IncCreateCommittedResource();
		const HRESULT hr = d->RootDevice->CreateCommittedResource(&HeapProps, D3D12_HEAP_FLAG_NONE, &InDesc, InitialUsage, ClearValue, IID_PPV_ARGS(pResource.get_init_ref()));

		if (SUCCEEDED(hr))
		{
			// Set a default name (can override later).
			pResource->SetName(Name);
			// Set the output pointer
			*ppOutResource = new FD3D12Resource(GetDevice(), pResource.get(), InitialUsage, InDesc, HeapProps.Type);
			(*ppOutResource)->AddRef();
		}

		return hr;
	}

	HRESULT FD3D12Adapter::CreateBuffer(D3D12_HEAP_TYPE HeapType, uint64_t HeapSize, FD3D12Resource** ppOutResource, 
		                               const wchar_t* Name, D3D12_RESOURCE_FLAGS Flags /*= D3D12_RESOURCE_FLAG_NONE*/)
	{
		const D3D12_HEAP_PROPERTIES HeapProps = CD3DX12_HEAP_PROPERTIES(HeapType);
		const D3D12_RESOURCE_STATES InitialState = DetermineInitialResourceState(HeapProps.Type, &HeapProps);
		return CreateBuffer(HeapProps, InitialState, HeapSize, ppOutResource, Name, Flags);
	}

	HRESULT FD3D12Adapter::CreateBuffer(D3D12_HEAP_TYPE HeapType, D3D12_RESOURCE_STATES InitialState, uint64_t HeapSize, 
									   FD3D12Resource** ppOutResource, const wchar_t* Name, D3D12_RESOURCE_FLAGS Flags /*= D3D12_RESOURCE_FLAG_NONE*/)
	{
		return E_FAIL;
	}

	HRESULT FD3D12Adapter::CreateBuffer(const D3D12_HEAP_PROPERTIES& HeapProps, D3D12_RESOURCE_STATES InitialState, 
								       uint64_t HeapSize, FD3D12Resource** ppOutResource, 
		                               const wchar_t* Name, D3D12_RESOURCE_FLAGS Flags /*= D3D12_RESOURCE_FLAG_NONE*/)
	{
		if (!ppOutResource)
		{
			return E_POINTER;
		}

		const D3D12_RESOURCE_DESC BufDesc = CD3DX12_RESOURCE_DESC::Buffer(HeapSize, Flags);
		return CreateCommittedResource(BufDesc,
			HeapProps,
			InitialState,
			nullptr,
			ppOutResource, Name);
	}

	bool FD3D12Adapter::CreateRootDevice(bool bWithDebug)
	{
		if (!CreateDXGIFactory())
			return false;;
		C_P(FD3D12Adapter);
		// QI for the Adapter
		win32::com_ptr<IDXGIAdapter> TempAdapter;
		d->DxgiFactory->EnumAdapters(d->Desc.AdapterIndex, TempAdapter.get_init_ref());
		HRESULT hr = TempAdapter->QueryInterface(IID_PPV_ARGS(d->DxgiAdapter.get_init_ref()));
		if (FAILED(hr))
		{
			core::logger::err() << __FUNCTION__" TempAdapter->QueryInterface failed:" << std::hex << hr;
			return false;
		}
		// In Direct3D 11, if you are trying to create a hardware or a software device, set pAdapter != NULL which constrains the other inputs to be:
		//		DriverType must be D3D_DRIVER_TYPE_UNKNOWN 
		//		Software must be NULL. 
		D3D_DRIVER_TYPE DriverType = D3D_DRIVER_TYPE_UNKNOWN;
		if (bWithDebug)
		{
			win32::com_ptr<ID3D12Debug> DebugController;
			if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(DebugController.get_init_ref()))))
			{
				const HRESULT hrDxgiDebug = DXGIGetDebugInterface1(0, IID_PPV_ARGS(d->DxgiDebug.get_init_ref()));
				if (FAILED(hrDxgiDebug))
				{
					core::logger::war() << __FUNCTION__
						<< " DXGIGetDebugInterface1 failed (optional: Windows Settings -> Apps -> Optional features -> Graphics Tools). HRESULT: "
						<< std::hex << hrDxgiDebug;
				}
				DebugController->EnableDebugLayer();

				// GBV injects many internal queues/heaps/resources; ReportLiveDeviceObjects will list them as "live".
				// Use d3d12_gpudev=0 with d3ddebug=1 when you only care about your app's objects.
				int gpudevFlag = 1;
				bool bEnableGpuValidation = true;
				if (core::CommandLine::Get().GetInteger("d3d12_gpudev", gpudevFlag))
					bEnableGpuValidation = (gpudevFlag != 0);

				win32::com_ptr<ID3D12Debug1> DebugController1;
				DebugController->QueryInterface(IID_PPV_ARGS(DebugController1.get_init_ref()));
				if (DebugController1)
				{
					DebugController1->SetEnableGPUBasedValidation(bEnableGpuValidation);
					if (bEnableGpuValidation)
						core::inf() << "D3D12 debug: GPU-based validation on (d3d12_gpudev=0 to reduce debug-layer live objects at shutdown)";
					else
						core::inf() << "D3D12 debug: GPU-based validation off (d3d12_gpudev=0)";
				}
			}
			else
			{
				bWithDebug = false;
			}
		}

		const bool bIsPerfHUD = false;
		if (bIsPerfHUD)
		{
			DriverType = D3D_DRIVER_TYPE_REFERENCE;
		}

		// Creating the Direct3D device.
		hr = ::D3D12CreateDevice(
			GetAdapter(),
			GetFeatureLevel(),
			IID_PPV_ARGS(d->RootDevice.get_init_ref())
		);
		if (FAILED(hr))
		{
			core::logger::err() << __FUNCTION__" D3D12CreateDevice failed:" << std::hex << hr;
			return false;
		}

		// Detect availability of shader model 6.0 wave operations
		{
			D3D12_FEATURE_DATA_D3D12_OPTIONS1 Features = {};
			d->RootDevice->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS1, &Features, sizeof(Features));
		}

		// Add some filter outs for known debug spew messages (that we don't care about)
		if (bWithDebug)
		{

			ID3D12InfoQueue* pd3dInfoQueue = nullptr;
			d->RootDevice->QueryInterface(__uuidof(ID3D12InfoQueue), (void**)&pd3dInfoQueue);
			if (pd3dInfoQueue)
			{
				D3D12_INFO_QUEUE_FILTER NewFilter;
				ZeroMemory(&NewFilter, sizeof(NewFilter));

				// Turn off info msgs as these get really spewy
				D3D12_MESSAGE_SEVERITY DenySeverity = D3D12_MESSAGE_SEVERITY_INFO;
				NewFilter.DenyList.NumSeverities = 1;
				NewFilter.DenyList.pSeverityList = &DenySeverity;

				// Be sure to carefully comment the reason for any additions here!  Someone should be able to look at it later and get an idea of whether it is still necessary.
				std::vector<D3D12_MESSAGE_ID> DenyIds = {
					// The Pixel Shader expects a Render Target View bound to slot 0, but the PSO indicates that none will be bound.
					// This typically happens when a non-depth-only pixel shader is used for depth-only rendering.
					D3D12_MESSAGE_ID_CREATEGRAPHICSPIPELINESTATE_RENDERTARGETVIEW_NOT_SET,

					// QUERY_BEGIN_ABANDONING_PREVIOUS_RESULTS - The RHI exposes the interface to make and issue queries and a separate interface to use that data.
					//		Currently there is a situation where queries are issued and the results may be ignored on purpose.  Filtering out this message so it doesn't
					//		swarm the debug spew and mask other important warnings
					//D3D12_MESSAGE_ID_QUERY_BEGIN_ABANDONING_PREVIOUS_RESULTS,
					//D3D12_MESSAGE_ID_QUERY_END_ABANDONING_PREVIOUS_RESULTS,

					// D3D12_MESSAGE_ID_CREATEINPUTLAYOUT_EMPTY_LAYOUT - This is a warning that gets triggered if you use a null vertex declaration,
					//       which we want to do when the vertex shader is generating vertices based on ID.
					D3D12_MESSAGE_ID_CREATEINPUTLAYOUT_EMPTY_LAYOUT,

					// D3D12_MESSAGE_ID_COMMAND_LIST_DRAW_INDEX_BUFFER_TOO_SMALL - This warning gets triggered by Slate draws which are actually using a valid index range.
					//		The invalid warning seems to only happen when VS 2012 is installed.  Reported to MS.  
					//		There is now an assert in DrawIndexedPrimitive to catch any valid errors reading from the index buffer outside of range.
					D3D12_MESSAGE_ID_COMMAND_LIST_DRAW_INDEX_BUFFER_TOO_SMALL,

					// D3D12_MESSAGE_ID_DEVICE_DRAW_RENDERTARGETVIEW_NOT_SET - This warning gets triggered by shadow depth rendering because the shader outputs
					//		a color but we don't bind a color render target. That is safe as writes to unbound render targets are discarded.
					//		Also, batched elements triggers it when rendering outside of scene rendering as it outputs to the GBuffer containing normals which is not bound.
					//(D3D12_MESSAGE_ID)3146081, // D3D12_MESSAGE_ID_DEVICE_DRAW_RENDERTARGETVIEW_NOT_SET,
					// BUGBUG: There is a D3D12_MESSAGE_ID_DEVICE_DRAW_DEPTHSTENCILVIEW_NOT_SET, why not one for RT?

					// D3D12_MESSAGE_ID_CLEARDEPTHSTENCILVIEW_MISMATCHINGCLEARVALUE/D3D12_MESSAGE_ID_CLEARRENDERTARGETVIEW_MISMATCHINGCLEARVALUE - 
					//      This warning gets triggered by ClearDepthStencilView/ClearRenderTargetView because when the resource was created
					//      it wasn't passed an optimized clear color (see CreateCommitedResource). This shows up a lot and is very noisy.
					D3D12_MESSAGE_ID_CLEARRENDERTARGETVIEW_MISMATCHINGCLEARVALUE,
					D3D12_MESSAGE_ID_CLEARDEPTHSTENCILVIEW_MISMATCHINGCLEARVALUE,

					// D3D12_MESSAGE_ID_EXECUTECOMMANDLISTS_GPU_WRITTEN_READBACK_RESOURCE_MAPPED - This warning gets triggered by ExecuteCommandLists.
					//		if it contains a readback resource that still has mapped subresources when executing a command list that performs a copy operation to the resource.
					//		This may be ok if any data read from the readback resources was flushed by calling Unmap() after the resourcecopy operation completed.
					//		We intentionally keep the readback resources persistently mapped.
					D3D12_MESSAGE_ID_EXECUTECOMMANDLISTS_GPU_WRITTEN_READBACK_RESOURCE_MAPPED,

					// Note message ID doesn't exist in the current header (yet, should be available in the RS2 header) for now just mute by the ID number.
					// RESOURCE_BARRIER_DUPLICATE_SUBRESOURCE_TRANSITIONS - This shows up a lot and is very noisy. It would require changes to the resource tracking system
					// but will hopefully be resolved when the RHI switches to use the engine's resource tracking system.
					(D3D12_MESSAGE_ID)1008,

					// This error gets generated on the first run when you install a new driver. The code handles this error properly and resets the PipelineLibrary,
					// so we can safely ignore this message. It could possibly be avoided by adding driver version to the PSO cache filename, but an average user is unlikely
					// to be interested in keeping PSO caches associated with old drivers around on disk, so it's better to just reset.
					D3D12_MESSAGE_ID_CREATEPIPELINELIBRARY_DRIVERVERSIONMISMATCH,
				};

#if D3D12_RHI_RAYTRACING
				if (bRayTracingSupported)
				{
					// When the debug layer is enabled and ray tracing is supported, this error is triggered after a CopyDescriptors
					// call in the DescriptorCache even when ray tracing device is never used. This workaround is still required as of 2018-12-17.
					DenyIds.Add(D3D12_MESSAGE_ID_COPY_DESCRIPTORS_INVALID_RANGES);
				}
#endif // D3D12_RHI_RAYTRACING

				NewFilter.DenyList.NumIDs = (UINT)DenyIds.size();
				NewFilter.DenyList.pIDList = DenyIds.data();

				pd3dInfoQueue->PushStorageFilter(&NewFilter);

				// Break on D3D debug errors.
				pd3dInfoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_ERROR, true);

				// Enable this to break on a specific id in order to quickly get a callstack
				//pd3dInfoQueue->SetBreakOnID(D3D12_MESSAGE_ID_DEVICE_DRAW_CONSTANT_BUFFER_TOO_SMALL, true);

				pd3dInfoQueue->Release();
			}
		}

		return true;
	}

	bool FD3D12Adapter::CreateDXGIFactory()
	{
		C_P(FD3D12Adapter);
		HRESULT hr = ::CreateDXGIFactory(IID_PPV_ARGS(d->DxgiFactory.get_init_ref()));
		if (FAILED(hr))
		{
			core::logger::err() << __FUNCTION__" CreateDXGIFactory failed:" << std::hex << hr;
			return false;
		}
		hr = d->DxgiFactory->QueryInterface(IID_PPV_ARGS(d->DxgiFactory2.get_init_ref()));
		if (FAILED(hr))
		{
			core::logger::err() << __FUNCTION__" QueryInterface(DxgiFactory2) failed:" << std::hex << hr;
			return false;
		}
		return true;
	}

	void FD3D12Adapter::Cleanup()
	{
		C_P(FD3D12Adapter);
	
		if (d->Device)
		{
			d->Device->Cleanup();
			d->Device = {};
		}
		if (d->FrameFence)
			d->FrameFence->Destroy();
		d->FrameFence = {};
		if(d->FenceCorePool)
			d->FenceCorePool->Destroy();
		d->FenceCorePool = {};
	}

	void FD3D12Adapter::BlockUntilIdle()
	{
		C_P(FD3D12Adapter);
		if (d->Device)
			d->Device->BlockUntilIdle();
	}

}