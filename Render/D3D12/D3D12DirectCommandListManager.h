#pragma once
#include "D3D12/D3D12RHICommon.h"
#include "d3dx12.h"
#include "D3D12/MultiGPU.h"
#include "D3D12/D3D12CommandList.h"
#include "core/memory_manager.h"

namespace RenderCore
{
	struct D3D12CommandListPayload
	{
		D3D12CommandListPayload() : NumCommandLists(0)
		{
			win32::Memzero(CommandLists);
		}

		void Reset();
		void Append(ID3D12CommandList* CL);

		static const uint32_t MaxCommandListsPerPayload = 256;
		ID3D12CommandList* CommandLists[MaxCommandListsPerPayload];
		uint32_t NumCommandLists;
	};

	enum class CommandListState
	{
		kOpen,
		kQueued,
		kFinished
	};

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

	// Fence value must be incremented manually. Useful when you need incrementing and signaling to happen at different times.
	class D3D12ManualFence : public D3D12Fence
	{
	public:
		explicit D3D12ManualFence(std::weak_ptr<D3D12Adapter> InParent, const std::wstring& InName = L"<unnamed>")
			: D3D12Fence(InParent, InName)
		{}

		// Signals the specified fence value.
		uint64_t Signal(ED3D12CommandQueueType InQueueType, uint64_t FenceToSignal);

		// Increments the current fence and returns the previous value.
		inline uint64_t IncrementCurrentFence() { return CurrentFence++; }
	};

	class D3D12Device;
	class D3D12CommandAllocator;
	class D3D12CommandAllocatorManager : public D3D12DeviceChild
	{
	public:
		D3D12CommandAllocatorManager(D3D12Device* InParent, const D3D12_COMMAND_LIST_TYPE& InType);
		~D3D12CommandAllocatorManager();

		D3D12CommandAllocator* ObtainCommandAllocator();
		void ReleaseCommandAllocator(D3D12CommandAllocator* CommandAllocator);

	private:
		std::vector<D3D12CommandAllocator*> CommandAllocators;		// List of all command allocators owned by this manager
		std::queue<D3D12CommandAllocator*> CommandAllocatorQueue;	// Queue of available allocators. Note they might still be in use by the GPU.
		std::recursive_mutex CS;	// Must be thread-safe because multiple threads can obtain/release command allocators
		const D3D12_COMMAND_LIST_TYPE Type;
	};

	class D3D12CommandListManager : public D3D12DeviceChild
	{
	public:
		struct FResolvedCmdListExecTime
		{
			uint64_t StartTimestamp;
			uint64_t EndTimestamp;

			FResolvedCmdListExecTime() = default;

			FResolvedCmdListExecTime(uint64_t InStart, uint64_t InEnd)
				: StartTimestamp(InStart)
				, EndTimestamp(InEnd)
			{}
		};

		D3D12CommandListManager(D3D12Device* InParent, D3D12_COMMAND_LIST_TYPE InCommandListType, ED3D12CommandQueueType InQueueType);
		virtual ~D3D12CommandListManager();

		void Create(const wchar_t* Name, uint32_t NumCommandLists = 0, uint32_t Priority = 0);
		void Destroy();

		inline bool IsReady()
		{
			return D3DCommandQueue.get() != nullptr;
		}

		// This use to also take an optional PSO parameter so that we could pass this directly to Create/Reset command lists,
		// however this was removed as we generally can't actually predict what PSO we'll need until draw due to frequent
		// state changes. We leave PSOs to always be resolved in ApplyState().
		D3D12CommandListHandle ObtainCommandList(D3D12CommandAllocator& CommandAllocator);
		void ReleaseCommandList(D3D12CommandListHandle& hList);

		void ExecuteCommandList(D3D12CommandListHandle& hList, bool WaitForCompletion = false);
		virtual void ExecuteCommandLists(std::vector<D3D12CommandListHandle>& Lists, bool WaitForCompletion = false);

		uint32_t GetResourceBarrierCommandList(D3D12CommandListHandle& hList, D3D12CommandListHandle& hResourceBarrierList);

		CommandListState GetCommandListState(const D3D12CLSyncPoint& hSyncPoint);

		bool IsComplete(const D3D12CLSyncPoint& hSyncPoint, uint64_t FenceOffset = 0);
		void WaitForCompletion(const D3D12CLSyncPoint& hSyncPoint)
		{
			hSyncPoint.WaitForCompletion();
		}

		FORCEINLINE HRESULT GetTimestampFrequency(uint64_t* Frequency) { return D3DCommandQueue->GetTimestampFrequency(Frequency); }
		FORCEINLINE ID3D12CommandQueue* GetD3DCommandQueue() { return D3DCommandQueue.get(); }
		FORCEINLINE ED3D12CommandQueueType GetQueueType() const { return QueueType; }

		FORCEINLINE D3D12Fence& GetFence() { assert(CommandListFence); return *CommandListFence; }

		void WaitForCommandQueueFlush();
		void ReleaseResourceBarrierCommandListAllocator();

	private:
		// Returns signaled Fence
		uint64_t ExecuteAndIncrementFence(D3D12CommandListPayload& Payload, D3D12Fence& Fence);
		D3D12CommandListHandle CreateCommandListHandle(D3D12CommandAllocator& CommandAllocator);
	private:
		win32::com_ptr<ID3D12CommandQueue>		D3DCommandQueue;

		ThreadsafeQueue<D3D12CommandListHandle> ReadyLists;

		// Command allocators used exclusively for resource barrier command lists.
		D3D12CommandAllocatorManager ResourceBarrierCommandAllocatorManager;
		D3D12CommandAllocator* ResourceBarrierCommandAllocator;

		std::shared_ptr<D3D12Fence> CommandListFence;

		D3D12_COMMAND_LIST_TYPE					CommandListType;
		ED3D12CommandQueueType					QueueType;
		std::recursive_mutex					ResourceStateCS;
		std::recursive_mutex					FenceCS;
	};

}