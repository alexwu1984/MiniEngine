#pragma once
#include "win/win32.h"
#include "RHI/RHIDefinitions.h"
#include "d3dx12.h"

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

	class D3D12Adapter : std::enable_shared_from_this<D3D12Adapter>
	{
	public:
		D3D12Adapter(const D3D12AdapterDesc& desc);
		~D3D12Adapter();

		void Initialize(std::weak_ptr<D3D12DynamicRHI> RHI);
		void InitializeDevices();
		void InitializeRayTracing();
		void SetDeviceRemoved(bool value);
		const bool IsDeviceRemoved() const;
		ID3D12Device* GetD3DDevice() const;
		ID3D12Device1* GetD3DDevice1() const;
		ID3D12Device2* GetD3DDevice2() const;
		FD3D12FenceCorePool& GetFenceCorePool();
		D3D12ManualFence& GetFrameFence();
		D3D12Device* GetDevice(uint32_t GPUIndex);
		std::shared_ptr<D3D12DynamicRHI> GetOwningRHI();
	protected:
		// Creates default root and execute indirect signatures
		void CreateSignatures();
	public:
		const uint32_t GetAdapterIndex() const;
		const D3D_FEATURE_LEVEL GetFeatureLevel() const;
		const DXGI_ADAPTER_DESC& GetD3DAdapterDesc() const;
		IDXGIAdapter* GetAdapter();
		const D3D12AdapterDesc& GetDesc();
	private:
		bool CreateRootDevice(bool bWithDebug);
		bool CreateDXGIFactory();
		void Cleanup();
	private:
		D3D12AdapterPrivate* d_ptr = nullptr;
	};
}