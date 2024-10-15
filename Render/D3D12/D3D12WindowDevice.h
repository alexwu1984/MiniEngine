#pragma once
#include "D3D12/D3D12RHICommon.h"
#include "d3dx12.h"

class D3D12CommandListManager;

namespace RenderCore
{
	class D3D12Device :public D3D12AdapterChild, public std::enable_shared_from_this<D3D12Device>
	{
	public:
		D3D12Device(std::weak_ptr<D3D12Adapter> InAdapter);
		virtual ~D3D12Device();

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
		inline D3D12CommandListManager& GetCommandListManager() { return *CommandListManager; }

	private:
		/** A pool of command lists we can cycle through for the global D3D device */
		D3D12CommandListManager* CommandListManager;
		D3D12CommandListManager* CopyCommandListManager;
		D3D12CommandListManager* AsyncCommandListManager;
	};
}