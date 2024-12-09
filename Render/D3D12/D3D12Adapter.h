#pragma once
#include "RHIPrivate/D3D12RHIPrivate.h"

namespace RenderCore
{
	struct FD3D12AdapterDesc
	{
		FD3D12AdapterDesc()
			: AdapterIndex(-1)
			, MaxSupportedFeatureLevel((D3D_FEATURE_LEVEL)0)
			, NumDeviceNodes(0)
		{
		}

		FD3D12AdapterDesc(DXGI_ADAPTER_DESC& DescIn, int32_t InAdapterIndex, D3D_FEATURE_LEVEL InMaxSupportedFeatureLevel, uint32_t NumNodes)
			: AdapterIndex(InAdapterIndex)
			, MaxSupportedFeatureLevel(InMaxSupportedFeatureLevel)
			, Desc(DescIn)
			, NumDeviceNodes(NumNodes)
		{
		}

		bool IsValid() const { return MaxSupportedFeatureLevel != (D3D_FEATURE_LEVEL)0 && AdapterIndex >= 0; }

		/** -1 if not supported or FindAdpater() wasn't called. Ideally we would store a pointer to IDXGIAdapter but it's unlikely the adpaters change during engine init. */
		int32_t AdapterIndex;
		/** The maximum D3D12 feature level supported. 0 if not supported or FindAdpater() wasn't called */
		D3D_FEATURE_LEVEL MaxSupportedFeatureLevel;

		DXGI_ADAPTER_DESC Desc;

		uint32_t NumDeviceNodes;
	};

	struct FD3D12AdapterPrivate;
	class D3D12DynamicRHI;
	class FD3D12FenceCorePool;
	class FD3D12ManualFence;
	class FD3D12Device;
	class FD3D12Fence;
	class FD3D12Resource;
	class FRootSignature;

	class FD3D12Adapter : public std::enable_shared_from_this<FD3D12Adapter>
	{
	public:
		FD3D12Adapter(const FD3D12AdapterDesc& desc);
		~FD3D12Adapter();

		void Initialize(const std::weak_ptr<D3D12DynamicRHI>& RHI);
		void InitializeDevices();
		void InitializeRayTracing();
		void SetDeviceRemoved(bool value);
		bool IsDeviceRemoved() const;
		ID3D12Device* GetD3DDevice() const;
		ID3D12Device1* GetD3DDevice1() const;
		ID3D12Device2* GetD3DDevice2() const;
		FD3D12FenceCorePool& GetFenceCorePool();
		FD3D12ManualFence& GetFrameFence();
		FD3D12Fence* GetStagingFence();
		std::shared_ptr<FD3D12Device> GetDevice(uint32_t GPUIndex);
		std::shared_ptr<D3D12DynamicRHI> GetOwningRHI();
		D3D12_RESOURCE_HEAP_TIER GetResourceHeapTier() const;
		D3D12_RESOURCE_BINDING_TIER GetResourceBindingTier() const;
		D3D_ROOT_SIGNATURE_VERSION GetRootSignatureVersion() const;
		bool IsDepthBoundsTestSupported() const;
		uint32_t GetAdapterIndex() const;
		D3D_FEATURE_LEVEL GetFeatureLevel() const;
		const DXGI_ADAPTER_DESC& GetD3DAdapterDesc() const;
		IDXGIAdapter* GetAdapter();
		const FD3D12AdapterDesc& GetDesc() const;
		IDXGIFactory* GetDXGIFactory() const;
		IDXGIFactory2* GetDXGIFactory2() const;

		std::shared_ptr<FRootSignature> GetRootSignature() const;

		// Resource Creation
		HRESULT CreateCommittedResource(const D3D12_RESOURCE_DESC& Desc,
			const D3D12_HEAP_PROPERTIES& HeapProps,
			const D3D12_RESOURCE_STATES& InitialUsage,
			const D3D12_CLEAR_VALUE* ClearValue,
			FD3D12Resource** ppOutResource,
			const wchar_t* Name);

		HRESULT CreateBuffer(D3D12_HEAP_TYPE HeapType,
			uint64_t HeapSize,
			FD3D12Resource** ppOutResource,
			const wchar_t* Name,
			D3D12_RESOURCE_FLAGS Flags = D3D12_RESOURCE_FLAG_NONE);

		HRESULT CreateBuffer(D3D12_HEAP_TYPE HeapType,
			D3D12_RESOURCE_STATES InitialState,
			uint64_t HeapSize,
			FD3D12Resource** ppOutResource,
			const wchar_t* Name,
			D3D12_RESOURCE_FLAGS Flags = D3D12_RESOURCE_FLAG_NONE);

		HRESULT CreateBuffer(const D3D12_HEAP_PROPERTIES& HeapProps,
			D3D12_RESOURCE_STATES InitialState,
			uint64_t HeapSize,
			FD3D12Resource** ppOutResource,
			const wchar_t* Name,
			D3D12_RESOURCE_FLAGS Flags = D3D12_RESOURCE_FLAG_NONE);

		void Cleanup();
	protected:
		// Creates default root and execute indirect signatures
		void CreateSignatures();

	private:
		bool CreateRootDevice(bool bWithDebug);
		bool CreateDXGIFactory();
		
	private:
		FD3D12AdapterPrivate* d_ptr = nullptr;
	};
}