#pragma once
#include "RHIPrivate/D3D12RHIPrivate.h"

namespace RenderCore
{
	struct D3D12AdapterDesc
	{
		D3D12AdapterDesc()
			: AdapterIndex(-1)
			, MaxSupportedFeatureLevel((D3D_FEATURE_LEVEL)0)
			, NumDeviceNodes(0)
		{
		}

		D3D12AdapterDesc(DXGI_ADAPTER_DESC& DescIn, int32_t InAdapterIndex, D3D_FEATURE_LEVEL InMaxSupportedFeatureLevel, uint32_t NumNodes)
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

	struct D3D12AdapterPrivate;
	class D3D12DynamicRHI;
	class FD3D12FenceCorePool;
	class D3D12ManualFence;
	class D3D12Device;
	class D3D12Fence;
	class D3D12Resource;

	class D3D12Adapter : std::enable_shared_from_this<D3D12Adapter>
	{
	public:
		D3D12Adapter(const D3D12AdapterDesc& desc);
		~D3D12Adapter();

		void Initialize(std::weak_ptr<D3D12DynamicRHI> RHI);
		void InitializeDevices();
		void InitializeRayTracing();
		void SetDeviceRemoved(bool value);
		FORCEINLINE const bool IsDeviceRemoved() const;
		FORCEINLINE ID3D12Device* GetD3DDevice() const;
		FORCEINLINE ID3D12Device1* GetD3DDevice1() const;
		FORCEINLINE ID3D12Device2* GetD3DDevice2() const;
		FORCEINLINE FD3D12FenceCorePool& GetFenceCorePool();
		FORCEINLINE D3D12ManualFence& GetFrameFence();
		FORCEINLINE D3D12Fence* GetStagingFence();
		FORCEINLINE D3D12Device* GetDevice(uint32_t GPUIndex);
		FORCEINLINE std::shared_ptr<D3D12DynamicRHI> GetOwningRHI();
		FORCEINLINE const D3D12_RESOURCE_HEAP_TIER GetResourceHeapTier() const;
		FORCEINLINE const D3D12_RESOURCE_BINDING_TIER GetResourceBindingTier() const;
		FORCEINLINE const D3D_ROOT_SIGNATURE_VERSION GetRootSignatureVersion() const;
		FORCEINLINE const bool IsDepthBoundsTestSupported() const;
		FORCEINLINE const uint32_t GetAdapterIndex() const;
		FORCEINLINE const D3D_FEATURE_LEVEL GetFeatureLevel() const;
		FORCEINLINE const DXGI_ADAPTER_DESC& GetD3DAdapterDesc() const;
		FORCEINLINE IDXGIAdapter* GetAdapter();
		FORCEINLINE const D3D12AdapterDesc& GetDesc() const;
		FORCEINLINE IDXGIFactory* GetDXGIFactory() const;
		FORCEINLINE IDXGIFactory2* GetDXGIFactory2() const;

		//FORCEINLINE std::vector<FD3D12Viewport*>& GetViewports() { return Viewports; }
		//FORCEINLINE FD3D12Viewport* GetDrawingViewport() { return DrawingViewport; }
		//FORCEINLINE void SetDrawingViewport(FD3D12Viewport* InViewport) { DrawingViewport = InViewport; }

		FORCEINLINE ID3D12CommandSignature* GetDrawIndirectCommandSignature();
		FORCEINLINE ID3D12CommandSignature* GetDrawIndexedIndirectCommandSignature();
		FORCEINLINE ID3D12CommandSignature* GetDispatchIndirectCommandSignature();

		//FORCEINLINE FD3D12PipelineStateCache& GetPSOCache() { return PipelineStateCache; }
		//FORCEINLINE FD3D12RootSignature* GetRootSignature(const FD3D12QuantizedBoundShaderState& QBSS)
		//{
		//	return RootSignatureManager.GetRootSignature(QBSS);
		//}
		//FORCEINLINE FD3D12RootSignatureManager* GetRootSignatureManager()
		//{
		//	return &RootSignatureManager;
		//}
		void EndFrame();

		// Resource Creation
		HRESULT CreateCommittedResource(const D3D12_RESOURCE_DESC& Desc,
			const D3D12_HEAP_PROPERTIES& HeapProps,
			const D3D12_RESOURCE_STATES& InitialUsage,
			const D3D12_CLEAR_VALUE* ClearValue,
			D3D12Resource** ppOutResource,
			const wchar_t* Name);

		HRESULT CreateBuffer(D3D12_HEAP_TYPE HeapType,
			uint64_t HeapSize,
			D3D12Resource** ppOutResource,
			const wchar_t* Name,
			D3D12_RESOURCE_FLAGS Flags = D3D12_RESOURCE_FLAG_NONE);

		HRESULT CreateBuffer(D3D12_HEAP_TYPE HeapType,
			D3D12_RESOURCE_STATES InitialState,
			uint64_t HeapSize,
			D3D12Resource** ppOutResource,
			const wchar_t* Name,
			D3D12_RESOURCE_FLAGS Flags = D3D12_RESOURCE_FLAG_NONE);

		HRESULT CreateBuffer(const D3D12_HEAP_PROPERTIES& HeapProps,
			D3D12_RESOURCE_STATES InitialState,
			uint64_t HeapSize,
			D3D12Resource** ppOutResource,
			const wchar_t* Name,
			D3D12_RESOURCE_FLAGS Flags = D3D12_RESOURCE_FLAG_NONE);

		void BlockUntilIdle();

	protected:
		// Creates default root and execute indirect signatures
		void CreateSignatures();

	private:
		bool CreateRootDevice(bool bWithDebug);
		bool CreateDXGIFactory();
		void Cleanup();
	private:
		D3D12AdapterPrivate* d_ptr = nullptr;
	};
}