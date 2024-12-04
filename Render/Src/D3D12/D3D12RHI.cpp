#include "D3D12/D3D12RHI.h"
#include "RHIPrivate/D3D12RHIPrivate.h"
#include "D3D12/D3D12WindowDevice.h"
#include "D3D12/D3D12Adapter.h"
#include "D3D12/D3D12CommandContext.h"
#include "math/math.h"
#include "core/timer.h"

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
		if (_DynamicRHI)
			return _DynamicRHI;
		FindAdapter();
		if (_ChosenAdapters.size())
			_DynamicRHI = std::make_shared<D3D12DynamicRHI>(_ChosenAdapters[0]);
		return _DynamicRHI;
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
		assert(_ChosenAdapters.size() == 0);

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
				_ChosenAdapters.push_back(NewAdapter);
			}
			else
			{
				NewAdapter = std::make_shared<FD3D12Adapter>(FD3D12Adapter(FirstAdapter));
				_ChosenAdapters.push_back(NewAdapter);
			}
		}
		else
		{
			NewAdapter = std::make_shared<FD3D12Adapter>(FD3D12Adapter(FirstAdapter));
			_ChosenAdapters.push_back(NewAdapter);
		}
	}

	D3D12DynamicRHI::D3D12DynamicRHI(std::shared_ptr<FD3D12Adapter> InAdapter)
		:D3D12Adapter(InAdapter)
	{

	}

	D3D12DynamicRHI::~D3D12DynamicRHI()
	{

	}

	void D3D12DynamicRHI::Init()
	{
		uint32_t Seed1 = core::Cycles();
		uint32_t Seed2 = core::Cycles();

		math::RandInit(Seed1);
		math::SRandInit(Seed2);

		D3D12Adapter->Initialize(this->shared_from_this());
		D3D12Adapter->InitializeDevices();
	}

	void D3D12DynamicRHI::Shutdown()
	{

	}

	std::shared_ptr<RHICommandContext> D3D12DynamicRHI::GetDefaultCommandContext()
	{
		if (!D3D12Adapter || !D3D12Adapter->GetDevice(0))
		{
			return {};
		}
		return D3D12Adapter->GetDevice(0)->GetDefaultCommandContext();
	}

	win32::com_ptr<ID3D12CommandQueue> D3D12DynamicRHI::CreateCommandQueue(std::weak_ptr<FD3D12Device> Device, const D3D12_COMMAND_QUEUE_DESC& Desc)
	{
		win32::com_ptr<ID3D12CommandQueue> pCommandQueue;
		VERIFYD3DRESULT(Device.lock()->GetDevice()->CreateCommandQueue(&Desc, IID_PPV_ARGS(pCommandQueue.get_init_ref())));
		return pCommandQueue;
	}

	std::shared_ptr<RHIVertexBuffer> D3D12DynamicRHI::RHICreateVertexBuffer(const void* InData, EBufferUsageFlags InUsage, int32_t StrideByteWidth, int32_t Count)
	{
		return {};
	}

	void D3D12DynamicRHI::RHIUpdateVertexBuffer(std::shared_ptr< RHIVertexBuffer> VertexBuffer, const void* InData, int32_t nVertex, int32_t sizePerVertex)
	{

	}

	std::shared_ptr<RHIIndexBuffer> D3D12DynamicRHI::RHICreateIndexBuffer(const uint16_t* InData, EBufferUsageFlags InUsage, int32_t IndexCount)
	{
		return {};
	}

	std::shared_ptr<RHIIndexBuffer> D3D12DynamicRHI::RHICreateIndexBuffer(const uint32_t* InData, EBufferUsageFlags InUsage, int32_t IndexCount)
	{
		return {};
	}

	std::shared_ptr<RHIUniformBuffer> D3D12DynamicRHI::RHICreateUniformBuffer(uint32_t ConstantBufferSize)
	{
		return {};
	}

	std::shared_ptr<RHIUniformBuffer> D3D12DynamicRHI::RHICreateUniformBuffer(const void* Contents, uint32_t ConstantBufferSize)
	{
		return {};
	}

	std::shared_ptr<RHITexture2D> D3D12DynamicRHI::RHICreateTexture2D(EPixelFormat format, int32_t Flags, int32_t width, int32_t height, uint32_t NumMips, void* pBuffer /*= nullptr*/, int rowBytes /*= 0*/)
	{
		return {};
	}

	std::shared_ptr<RHITexture2D> D3D12DynamicRHI::RHICreateTexture2D(const std::wstring& FileName)
	{
		return {};
	}

	std::shared_ptr<RHITexture2D> D3D12DynamicRHI::RHICreateTexture2D(const core::FLinearColor& Color)
	{
		return {};
	}

	std::shared_ptr<RHITexture2D> D3D12DynamicRHI::RHICreateHDRTexture2D(const std::wstring& FileName)
	{
		return {};
	}

	std::shared_ptr<RHITexture1D> D3D12DynamicRHI::RHICreateTexture1D(EPixelFormat Format, int32_t Flags, int32_t SizeX, void* InBuffer, int RowBytes)
	{
		return {};
	}

	std::shared_ptr<RHITextureCube> D3D12DynamicRHI::RHICreateTextureCube(EPixelFormat Format, int32_t SizeX, int32_t SizeY, uint32_t NumMips, bool CreateDepth)
	{
		return {};
	}

	std::shared_ptr<RHIUnorderedAccessView> D3D12DynamicRHI::RHICreateUnorderedAccessView(EPixelFormat Format, int32_t SizeX, int32_t SizeY)
	{
		return {};
	}

	std::shared_ptr<RHIUnorderedAccessView> D3D12DynamicRHI::RHICreateUnorderedAccessView(std::shared_ptr< RHITexture2D> Tex2D)
	{
		return {};
	}

	std::shared_ptr<RHIRenderTarget> D3D12DynamicRHI::RHICreateRenderTarget(EPixelFormat Format, int32_t SizeX, int32_t SizeY, uint32_t NumMips, bool IsMultiSampled, bool CreateDepth)
	{
		return {};
	}

	std::shared_ptr<RHIVertexShader> D3D12DynamicRHI::RHICreateVertexShader(const std::wstring& FileName, const std::string& VSMain, 
																			const RHIVertexDeclare& VertexDeclare, const std::vector<RHIShaderMacro>& MacroDefines)
	{
		return {};
	}

	std::shared_ptr<RHIPixelShader> D3D12DynamicRHI::RHICreatePixelShader(const std::wstring& FileName, const std::string& PSMain, const std::vector<RHIShaderMacro>& MacroDefines)
	{
		return {};
	}

	std::shared_ptr<RHIComputeShader> D3D12DynamicRHI::RHICreateComputeShader(const std::wstring& FileName, const std::string& CSMain, const std::vector<RHIShaderMacro>& MacroDefines)
	{
		return {};
	}

	std::shared_ptr<RHISamplerState> D3D12DynamicRHI::RHICreateSamplerState(const SamplerStateInitializerRHI& Initializer)
	{
		return {};
	}

	std::shared_ptr<RHIRasterizerState> D3D12DynamicRHI::RHICreateRasterizerState(const RasterizerStateInitializerRHI& Initializer)
	{
		return {};
	}

	std::shared_ptr<RHIBlendState> D3D12DynamicRHI::RHICreateBlendState(const BlendStateInitializerRHI& Initializer)
	{
		return {};
	}

	std::shared_ptr<RHIDepthStencilState> D3D12DynamicRHI::RHICreateDepthStencilState(const DepthStencilStateInitializerRHI& Initializer)
	{
		return {};
	}

	std::shared_ptr<RHITilePool> D3D12DynamicRHI::RHICreateTilePool(std::shared_ptr< RHITexture2D> Tex2D)
	{
		return {};
	}

}