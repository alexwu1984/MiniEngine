#include "D3D12/D3D12Adapter.h"
#include "win/com_ptr.h"
#include "D3D12/D3D12RHI.h"
#include "RHIPrivate/D3D12RHIPrivate.h"
#include "core/logger.h"

namespace RenderCore
{
	struct D3D12AdapterPrivate
	{
		// LDA setups have one ID3D12Device
		std::weak_ptr<D3D12DynamicRHI> OwningRHI;
		win32::com_ptr<ID3D12Device> RootDevice;
		win32::com_ptr<ID3D12Device1> RootDevice1;
		win32::com_ptr<ID3D12Device2> RootDevice2;
		win32::com_ptr<IDXGIFactory> DxgiFactory;
		win32::com_ptr<IDXGIFactory2> DxgiFactory2;
		win32::com_ptr<IDXGIAdapter> DxgiAdapter;
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
		d->OwningRHI = RHI;
	}

	void D3D12Adapter::InitializeDevices()
	{

	}

	void D3D12Adapter::InitializeRayTracing()
	{

	}

	const uint32_t D3D12Adapter::GetAdapterIndex() const
	{
		C_P(const D3D12Adapter);
		return d->Desc.AdapterIndex;
	}

	const D3D_FEATURE_LEVEL D3D12Adapter::GetFeatureLevel() const
	{
		C_P(const D3D12Adapter);
		return d->Desc.MaxSupportedFeatureLevel;
	}

	const DXGI_ADAPTER_DESC& D3D12Adapter::GetD3DAdapterDesc() const
	{
		C_P(const D3D12Adapter);
		return d->Desc.Desc;
	}

	IDXGIAdapter* D3D12Adapter::GetAdapter()
	{
		C_P(D3D12Adapter);
		return d->DxgiAdapter.get();
	}

	const D3D12AdapterDesc& D3D12Adapter::GetDesc()
	{
		C_P(const D3D12Adapter);
		return d->Desc;
	}

	bool D3D12Adapter::CreateRootDevice(bool bWithDebug)
	{
		if (!CreateDXGIFactory())
			return false;;
		C_P(D3D12Adapter);
		// QI for the Adapter
		win32::com_ptr<IDXGIAdapter> TempAdapter;
		d->DxgiFactory->EnumAdapters(d->Desc.AdapterIndex, TempAdapter.get_init_ref());
		HRESULT hr = TempAdapter->QueryInterface(IID_PPV_ARGS(d->DxgiAdapter.get_init_ref()));

		// In Direct3D 11, if you are trying to create a hardware or a software device, set pAdapter != NULL which constrains the other inputs to be:
		//		DriverType must be D3D_DRIVER_TYPE_UNKNOWN 
		//		Software must be NULL. 
		D3D_DRIVER_TYPE DriverType = D3D_DRIVER_TYPE_UNKNOWN;
		if (bWithDebug)
		{
			win32::com_ptr<ID3D12Debug> DebugController;
			if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(DebugController.get_init_ref()))))
			{
				DebugController->EnableDebugLayer();

				bool bD3d12gpuvalidation = false;
				//if (FParse::Param(FCommandLine::Get(), TEXT("d3d12gpuvalidation")) || FParse::Param(FCommandLine::Get(), TEXT("gpuvalidation")))
				{
					win32::com_ptr<ID3D12Debug1> DebugController1;
					DebugController->QueryInterface(IID_PPV_ARGS(DebugController1.get_init_ref()));
					if(DebugController1)
						DebugController1->SetEnableGPUBasedValidation(true);
					bD3d12gpuvalidation = true;
				}

				//UE_LOG(LogD3D12RHI, Log, TEXT("InitD3DDevice: -D3DDebug = %s -D3D12GPUValidation = %s"), bWithDebug ? TEXT("on") : TEXT("off"), bD3d12gpuvalidation ? TEXT("on") : TEXT("off"));
			}
			//else
			//{
			//	bWithDebug = false;
			//	UE_LOG(LogD3D12RHI, Fatal, TEXT("The debug interface requires the D3D12 SDK Layers. Please install the Graphics Tools for Windows. See: https://docs.microsoft.com/en-us/windows/uwp/gaming/use-the-directx-runtime-and-visual-studio-graphics-diagnostic-features"));
			//}
		}

		const bool bIsPerfHUD = false;
		if (bIsPerfHUD)
		{
			DriverType = D3D_DRIVER_TYPE_REFERENCE;
		}

		// Creating the Direct3D device.
		D3D12CreateDevice(
			GetAdapter(),
			GetFeatureLevel(),
			IID_PPV_ARGS(d->RootDevice.get_init_ref())
		);

		// Detect availability of shader model 6.0 wave operations
		{
			D3D12_FEATURE_DATA_D3D12_OPTIONS1 Features = {};
			d->RootDevice->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS1, &Features, sizeof(Features));
		}

		win32::com_ptr<ID3D12Debug> d3dDebug;
		if (SUCCEEDED(d->RootDevice->QueryInterface(__uuidof(ID3D12Debug), (void**)d3dDebug.get_init_ref())))
		{
			win32::com_ptr<ID3D12InfoQueue> d3dInfoQueue;
			if (SUCCEEDED(d3dDebug->QueryInterface(__uuidof(ID3D12InfoQueue), (void**)d3dInfoQueue.get_init_ref())))
			{
				d3dInfoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_CORRUPTION, true);
				d3dInfoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_ERROR, true);
				//d3dInfoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_WARNING, true);
			}
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

	#if ENABLE_RESIDENCY_MANAGEMENT
					// TODO: Remove this when the debug layers work for executions which are guarded by a fence
					D3D12_MESSAGE_ID_INVALID_USE_OF_NON_RESIDENT_RESOURCE,
	#endif
				};

#if D3D12_RHI_RAYTRACING
				if (bRayTracingSupported)
				{
					// When the debug layer is enabled and ray tracing is supported, this error is triggered after a CopyDescriptors
					// call in the DescriptorCache even when ray tracing device is never used. This workaround is still required as of 2018-12-17.
					DenyIds.Add(D3D12_MESSAGE_ID_COPY_DESCRIPTORS_INVALID_RANGES);
				}
#endif // D3D12_RHI_RAYTRACING

				NewFilter.DenyList.NumIDs = DenyIds.size();
				NewFilter.DenyList.pIDList = DenyIds.data();

				pd3dInfoQueue->PushStorageFilter(&NewFilter);

				// Break on D3D debug errors.
				pd3dInfoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_ERROR, true);

				// Enable this to break on a specific id in order to quickly get a callstack
				//pd3dInfoQueue->SetBreakOnID(D3D12_MESSAGE_ID_DEVICE_DRAW_CONSTANT_BUFFER_TOO_SMALL, true);

				//if (FParse::Param(FCommandLine::Get(), TEXT("d3dbreakonwarning")))
				//{
				//	pd3dInfoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_WARNING, true);
				//}

				pd3dInfoQueue->Release();
			}
		}

		return true;
	}

	bool D3D12Adapter::CreateDXGIFactory()
	{
		C_P(D3D12Adapter);
		HRESULT hr = ::CreateDXGIFactory(IID_PPV_ARGS(d->DxgiFactory.get_init_ref()));
		if (FAILED(hr))
		{
			core::logger::err() << std::hex << hr;
			return false;
		}
		hr = d->DxgiFactory->QueryInterface(IID_PPV_ARGS(d->DxgiFactory2.get_init_ref()));
		if (FAILED(hr))
		{
			core::logger::err() << std::hex << hr;
			return false;
		}
		return true;
	}

}