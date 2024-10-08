#include "D3D12/D3D12DirectCommandListManager.h"
#include "D3D12/D3D12Adapter.h"

namespace RenderCore
{
	D3D12FenceCore::D3D12FenceCore(std::weak_ptr<D3D12Adapter> Parent, uint64_t InitialValue, uint32_t GPUIndex)
		:D3D12AdapterChild(Parent)
	{
		assert(!Parent.expired());
		hFenceCompleteEvent = CreateEvent(nullptr, false, false, nullptr);
		assert(INVALID_HANDLE_VALUE != hFenceCompleteEvent);

		HRESULT hr = Parent.lock()->GetD3DDevice()->CreateFence(InitialValue, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(Fence.get_init_ref()));
		assert(SUCCEEDED(hr));
	}

	D3D12FenceCore::~D3D12FenceCore()
	{
		if (hFenceCompleteEvent != INVALID_HANDLE_VALUE)
		{
			CloseHandle(hFenceCompleteEvent);
			hFenceCompleteEvent = INVALID_HANDLE_VALUE;
		}
	}

	D3D12FenceCore* FD3D12FenceCorePool::ObtainFenceCore(uint32_t GPUIndex)
	{
		{
			std::lock_guard<std::recursive_mutex> lock(CS);
			D3D12FenceCore* Fence = nullptr;
			if (!AvailableFences[GPUIndex].empty() && Fence->IsAvailable())
			{
				Fence = AvailableFences[GPUIndex].front();
				AvailableFences[GPUIndex].pop();
				return Fence;
			}
		}

		return new D3D12FenceCore(GetParentAdapter(), 0, GPUIndex);
	}

	void FD3D12FenceCorePool::ReleaseFenceCore(D3D12FenceCore* Fence, uint64_t CurrentFenceValue)
	{
		std::lock_guard<std::recursive_mutex> lock(CS);
		Fence->FenceValueAvailableAt = CurrentFenceValue;
		AvailableFences[Fence->GetGPUIndex()].push(Fence);
	}

	void FD3D12FenceCorePool::Destroy()
	{
		for (uint32_t GPUIndex = 0; GPUIndex < MAX_NUM_GPUS; ++GPUIndex)
		{
			D3D12FenceCore* Fence = nullptr;
			while (!AvailableFences[GPUIndex].empty())
			{
				Fence = AvailableFences[GPUIndex].front();
				delete Fence;
				AvailableFences[GPUIndex].pop();
			}
		}
	}

	D3D12Fence::D3D12Fence(std::weak_ptr<D3D12Adapter> InParent, const std::wstring& InName /*= L"<unnamed>"*/)
		:D3D12AdapterChild(InParent)
		, Name(InName)
		, CurrentFence(0)
		, LastSignaledFence(0)
		, LastCompletedFence(0)
	{
		ZeroMemory(FenceCores, sizeof(FenceCores));
		ZeroMemory(LastCompletedFences, sizeof(LastCompletedFences));
	}

	D3D12Fence::~D3D12Fence()
	{
		Destroy();
	}

	void D3D12Fence::CreateFence()
	{
		// Can't set the last signaled fence per GPU before a common signal is sent.
		LastSignaledFence = 0;
		const uint32_t GPUIndex = 0;
		assert(!FenceCores[GPUIndex]);

		// Get a fence from the pool
		D3D12FenceCore* FenceCore = GetParentAdapter()->GetFenceCorePool().ObtainFenceCore(GPUIndex);
		assert(FenceCore);
		FenceCores[GPUIndex] = FenceCore;

		LastCompletedFences[GPUIndex] = FenceCore->FenceValueAvailableAt;

		LastCompletedFence = LastCompletedFences[GPUIndex];
		CurrentFence = LastCompletedFences[GPUIndex] + 1;
	}

	uint64_t D3D12Fence::Signal(ED3D12CommandQueueType InQueueType)
	{
		assert(LastSignaledFence != CurrentFence);
		InternalSignal(InQueueType, CurrentFence);

		// Update the cached version of the fence value
		UpdateLastCompletedFence();

		// Increment the current Fence
		CurrentFence++;

		return LastSignaledFence;
	}

	void D3D12Fence::GpuWait(uint32_t DeviceGPUIndex, ED3D12CommandQueueType InQueueType, uint64_t FenceValue, uint32_t FenceGPUIndex)
	{

	}

	void D3D12Fence::GpuWait(ED3D12CommandQueueType InQueueType, uint64_t FenceValue)
	{

	}

	bool D3D12Fence::IsFenceComplete(uint64_t FenceValue)
	{
		return false;
	}

	void D3D12Fence::WaitForFence(uint64_t FenceValue)
	{

	}

	void D3D12Fence::Destroy()
	{
		constexpr int32_t GPUIndex = 0;
		if (FenceCores[GPUIndex])
		{
			// Return the underlying fence to the pool, store the last value signaled on this fence. 
			// If not fence was signaled since CreateFence() was called, then the last completed value is the last signaled value for this GPU.
			GetParentAdapter()->GetFenceCorePool().ReleaseFenceCore(FenceCores[GPUIndex], LastSignaledFence > 0 ? LastSignaledFence : LastCompletedFences[GPUIndex]);

			FenceCores[GPUIndex] = nullptr;
		}
	}

	void D3D12Fence::InternalSignal(ED3D12CommandQueueType InQueueType, uint64_t FenceToSignal)
	{
		const uint32_t GPUIndex = 0;
		//ID3D12CommandQueue* CommandQueue = GetParentAdapter()->GetDevice(GPUIndex)->GetD3DCommandQueue(InQueueType);
		//assert(CommandQueue);
		//D3D12FenceCore* FenceCore = FenceCores[GPUIndex];
		//assert(FenceCore);

		//HRESULT hr = CommandQueue->Signal(FenceCore->GetFence(), FenceToSignal);
		//assert(SUCCEEDED(hr));
		LastSignaledFence = FenceToSignal;
	}

}