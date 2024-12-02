#pragma once
#include "D3D12/D3D12RHICommon.h"
#include "D3D12/D3D12DescriptorCache.h"

namespace RenderCore
{
	class FD3D12CommandListManager;
	class FD3D12Device :public FD3D12AdapterChild, public std::enable_shared_from_this<FD3D12Device>
	{
	public:
		FD3D12Device(std::weak_ptr<FD3D12Adapter> InAdapter);
		virtual ~FD3D12Device();

		/** Initialized members*/
		void Initialize();

		void CreateCommandContexts();

		void InitPlatformSpecific();
		/**
		* Cleanup the device.
		* This function must be called from the main game thread.
		*/
		virtual void Cleanup();

		ID3D12Device* GetDevice();

		ID3D12CommandQueue* GetD3DCommandQueue(ED3D12CommandQueueType InQueueType = ED3D12CommandQueueType::Default) const;
		inline FD3D12CommandListManager& GetCommandListManager() { return *CommandListManager; }

		void BlockUntilIdle();

	private:
		// shared code for different D3D12  devices (e.g. PC DirectX12 and XboxOne) called
		// after device creation and GRHISupportsAsyncTextureCreation was set and before resource init
		void SetupAfterDeviceCreation();

	private:
		/** A pool of command lists we can cycle through for the global D3D device */
		FD3D12CommandListManager* CommandListManager;
		FD3D12CommandListManager* CopyCommandListManager;
		FD3D12CommandListManager* AsyncCommandListManager;

		// Must be before the StateCache so that destructor ordering is valid
		FD3D12OfflineDescriptorManager RTVAllocator;
		FD3D12OfflineDescriptorManager DSVAllocator;
		FD3D12OfflineDescriptorManager SRVAllocator;
		FD3D12OfflineDescriptorManager UAVAllocator;
		FD3D12OfflineDescriptorManager SamplerAllocator;
	};
}