#pragma once
#include "D3D12/D3D12RHICommon.h"
#include "D3D12/D3D12DescriptorCache.h"

namespace RenderCore
{
	class FD3D12CommandListManager;
	class D3D11CommandContext;

	class FD3D12Device :public std::enable_shared_from_this<FD3D12Device>,public FD3D12AdapterChild
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
		FD3D12CommandListManager& GetCommandListManager() const { return *CommandListManager; }
		FD3D12CommandListManager& GetAsyncCommandListManager() const { return *AsyncCommandListManager; }
		std::shared_ptr<D3D12CommandContext> GetDefaultCommandContext() const { return DefaultCommandContext;}
		D3D12CommandContext& GetDefaultAsyncComputeContext() const { return *AsyncComputeContext; }

		void BlockUntilIdle();

	private:
		/** A pool of command lists we can cycle through for the global D3D device */
		std::shared_ptr<FD3D12CommandListManager> CommandListManager;
		std::shared_ptr<FD3D12CommandListManager> CopyCommandListManager;
		std::shared_ptr<FD3D12CommandListManager> AsyncCommandListManager;
		std::shared_ptr<D3D12CommandContext> DefaultCommandContext;
		std::shared_ptr<D3D12CommandContext> AsyncComputeContext;
	};
}