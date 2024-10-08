#pragma once
#include "D3D12/D3D12RHICommon.h"
#include "win/com_ptr.h"
#include "d3dx12.h"
#include "D3D12/MultiGPU.h"

namespace RenderCore
{
	class D3D12FenceCore : public D3D12AdapterChild
	{
	public:
		D3D12FenceCore(std::weak_ptr<D3D12Adapter> Parent, uint64_t InitialValue, uint32_t GPUIndex);
		~D3D12FenceCore();

		inline ID3D12Fence* GetFence() const { return Fence.get(); }
		inline HANDLE GetCompletionEvent() const { return hFenceCompleteEvent; }
		inline bool IsAvailable() const { return FenceValueAvailableAt <= Fence->GetCompletedValue(); }
		inline uint32_t GetGPUIndex() const { return GPUIndex; }

		uint64_t FenceValueAvailableAt;

	private:
		uint32_t GPUIndex;

		win32::com_ptr<ID3D12Fence> Fence;
		HANDLE hFenceCompleteEvent;
	};

	class FD3D12FenceCorePool : public D3D12AdapterChild
	{
	public:
		FD3D12FenceCorePool(std::weak_ptr<D3D12Adapter> Parent) : D3D12AdapterChild(Parent) {};

		D3D12FenceCore* ObtainFenceCore(uint32_t GPUIndex);
		void ReleaseFenceCore(D3D12FenceCore* Fence, uint64_t CurrentFenceValue);
		void Destroy();
	private:
		std::recursive_mutex CS;
		std::queue<D3D12FenceCore*> AvailableFences[MAX_NUM_GPUS];
	};

	class D3D12Fence : public D3D12AdapterChild
	{
	public:
		D3D12Fence(std::weak_ptr<D3D12Adapter> InParent, const std::wstring& InName = L"<unnamed>");
		~D3D12Fence();

		void CreateFence();
		uint64_t Signal(ED3D12CommandQueueType InQueueType);
		void GpuWait(uint32_t DeviceGPUIndex, ED3D12CommandQueueType InQueueType, uint64_t FenceValue, uint32_t FenceGPUIndex);
		void GpuWait(ED3D12CommandQueueType InQueueType, uint64_t FenceValue);
		bool IsFenceComplete(uint64_t FenceValue);
		void WaitForFence(uint64_t FenceValue);

		FORCEINLINE std::wstring GetName() const
		{
			return Name;
		}

		FORCEINLINE bool GetWriteEnqueued() const
		{
			return bWriteEnqueued;
		}

		virtual void Reset()
		{
			bWriteEnqueued = false;
		}

		virtual void WriteFence()
		{
			//ensureMsgf(!bWriteEnqueued, TEXT("ComputeFence: %s already written this frame. You should use a new label"), *Name.ToString());
			bWriteEnqueued = true;
		}

		// Avoids calling GetCompletedValue().
		bool IsFenceCompleteFast(uint64_t FenceValue) const { return FenceValue <= LastCompletedFence; }

		uint64_t GetCurrentFence() const { return CurrentFence; }
		uint64_t GetLastSignaledFence() const { return LastSignaledFence; }

		uint64_t PeekLastCompletedFence() const;
		uint64_t UpdateLastCompletedFence();

		// Might not be the most up to date value but avoids calling GetCompletedValue().
		uint64_t GetLastCompletedFenceFast() const { return LastCompletedFence; };

		void Destroy();

	protected:
		void InternalSignal(ED3D12CommandQueueType InQueueType, uint64_t FenceToSignal);

	protected:

		uint64_t CurrentFence;
		uint64_t LastSignaledFence; // 0 when not yet issued, otherwise the last value signaled to all GPU
		uint64_t LastCompletedFence; // The min value completed between all LastCompletedFences.
		std::recursive_mutex WaitForFenceCS;

		//only one for now
		uint64_t LastCompletedFences[MAX_NUM_GPUS];
		D3D12FenceCore* FenceCores[MAX_NUM_GPUS];

		//debug name of the label.
		std::wstring Name;

		//has the label been written to since being created.
		//check this when queuing waits to catch GPU hangs on the CPU at command creation time.
		bool bWriteEnqueued = false;
	};
}