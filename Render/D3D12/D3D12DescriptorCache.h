#pragma once
#include "RHIPrivate/D3D12RHIPrivate.h"
#include "Templates/UnrealTypeTraits.h"
#include "D3D12/MultiGPU.h"
#include "D3D12/D3D12CommandList.h"
#include <queue>
#include <vector>

namespace RenderCore
{
	class FRootSignature;

	struct FRetiredDynamicDescriptorHeapEntry
	{
		uint64_t FenceValue = 0;
		ED3D12CommandQueueType QueueType = ED3D12CommandQueueType::Default;
		win32::com_ptr<ID3D12DescriptorHeap> Heap;
	};

	// Shader-visible CBV/SRV/UAV vs sampler pools, owned by FD3D12Device (not process-global statics).
	struct FDynamicDescriptorHeapPoolsPerDevice
	{
		std::queue<win32::com_ptr<ID3D12DescriptorHeap>> Ready[2];
		// Keep separate retired queues per D3D12 queue type to preserve monotonic fence ordering
		// within each queue, enabling O(1) "while front complete" recycling like the DEMO.
		// Retired heaps store the fence value + queue that submitted the work that last touched them;
		// recycling checks that queue's fence (monotonic) before moving heap back to Ready.
		std::queue<FRetiredDynamicDescriptorHeapEntry> Retired[2][3]; // [HeapTypeIndex][QueueTypeIndex]
		std::vector<win32::com_ptr<ID3D12DescriptorHeap>> CreatedTracking[2];

		void Clear();
	};

	class FDescriptorHandle
	{
	public:
		FDescriptorHandle()
		{
			m_CpuHandle.ptr = D3D12_CPU_VIRTUAL_ADDRESS_UNKNOWN;
			m_GpuHandle.ptr = D3D12_GPU_VIRTUAL_ADDRESS_UNKNOWN;
		}

		FDescriptorHandle(D3D12_CPU_DESCRIPTOR_HANDLE CpuHandle)
			: m_CpuHandle(CpuHandle)
		{
			m_GpuHandle.ptr = D3D12_GPU_VIRTUAL_ADDRESS_UNKNOWN;
		}

		FDescriptorHandle(D3D12_CPU_DESCRIPTOR_HANDLE CpuHandle, D3D12_GPU_DESCRIPTOR_HANDLE GpuHandle)
			: m_CpuHandle(CpuHandle)
			, m_GpuHandle(GpuHandle)
		{}

		FDescriptorHandle(ID3D12DescriptorHeap* Heap)
		{
			m_CpuHandle = Heap->GetCPUDescriptorHandleForHeapStart();
			m_GpuHandle = Heap->GetGPUDescriptorHandleForHeapStart();
		}

		FDescriptorHandle operator + (INT Offset) const
		{
			FDescriptorHandle Result = *this;
			Result += Offset;
			return Result;
		}

		void operator += (INT Offset)
		{
			if (m_CpuHandle.ptr != D3D12_CPU_VIRTUAL_ADDRESS_UNKNOWN)
				m_CpuHandle.ptr += Offset;
			if (m_GpuHandle.ptr != D3D12_GPU_VIRTUAL_ADDRESS_UNKNOWN)
				m_GpuHandle.ptr += Offset;
		}

		D3D12_CPU_DESCRIPTOR_HANDLE GetCpuHandle() const { return m_CpuHandle; }
		D3D12_GPU_DESCRIPTOR_HANDLE GetGpuHandle() const { return m_GpuHandle; }

		bool IsCpuNull() const { return m_CpuHandle.ptr == D3D12_CPU_VIRTUAL_ADDRESS_UNKNOWN; }
		bool IsShaderVisible() const { return m_GpuHandle.ptr != D3D12_GPU_VIRTUAL_ADDRESS_UNKNOWN; }
		/** CPU/GPU both known and non-null; 0 is not a valid descriptor heap start on Windows. */
		bool IsValidShaderVisibleTableBase() const;

	private:
		D3D12_CPU_DESCRIPTOR_HANDLE m_CpuHandle;
		D3D12_GPU_DESCRIPTOR_HANDLE m_GpuHandle;
	};

	class FDynamicDescriptorHeap : public FD3D12DeviceChild
	{
	public:
		FDynamicDescriptorHeap(std::weak_ptr<FD3D12Device> InDevice, 
							   std::weak_ptr<D3D12CommandContext> CommandContext, 
							   D3D12_DESCRIPTOR_HEAP_TYPE HeapType);
		~FDynamicDescriptorHeap() = default;

		D3D12_GPU_DESCRIPTOR_HANDLE UploadDirect(D3D12_CPU_DESCRIPTOR_HANDLE Handle);

		void ParseGraphicsRootSignature(const FRootSignature& RootSignature)
		{
			m_GraphicsHandleCache.ParseRootSignature(m_HeapType, RootSignature);
		}

		void ParseComputeRootSignature(const FRootSignature& RootSignature)
		{
			m_ComputeHandleCache.ParseRootSignature(m_HeapType, RootSignature);
		}

		void SetGraphicsDescriptorHandles(UINT RootIndex, UINT Offset, UINT Count, const D3D12_CPU_DESCRIPTOR_HANDLE Handles[])
		{
			m_GraphicsHandleCache.StageDescriptorHandles(RootIndex, Offset, Count, Handles);
		}

		void SetComputeDescriptorHandles(UINT RootIndex, UINT Offset, UINT Count, const D3D12_CPU_DESCRIPTOR_HANDLE Handles[])
		{
			m_ComputeHandleCache.StageDescriptorHandles(RootIndex, Offset, Count, Handles);
		}

		void CommitGraphicsRootDescriptorTables(ID3D12GraphicsCommandList* CommandList);
		void CommitComputeRootDescriptorTables(ID3D12GraphicsCommandList* CommandList);

		void CleanupUsedHeaps(uint64_t FenceValue, ED3D12CommandQueueType QueueType);
		win32::com_ptr<ID3D12DescriptorHeap> GetHeapPointer();
	private:
		bool HasSpace(uint32_t Count)
		{
			return (m_CurrentHeap != nullptr && m_CurrentOffset + Count <= m_NumDescriptorsPerHeap);
		}
		void RetireCurrentHeap();
		void RetireUsedHeaps(uint64_t FenceValue, ED3D12CommandQueueType QueueType);
		void UnbindAllInvalid();
		win32::com_ptr<ID3D12DescriptorHeap> RequestDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE HeapType);
		FDynamicDescriptorHeapPoolsPerDevice& Pools();

	private:
		// Shader-visible ring: size is chosen per device (ResourceBindingTier) so we suballocate longer
		// in one heap before retiring — closer to UE's large heap + ring than tiny per-chunk heaps.
		uint32_t m_NumDescriptorsPerHeap = 16384;

		std::shared_ptr<D3D12CommandContext> m_OwningContext;
		win32::com_ptr<ID3D12DescriptorHeap> m_CurrentHeap;
		const D3D12_DESCRIPTOR_HEAP_TYPE m_HeapType;
		FDescriptorHandle m_FirstDescriptor;
		uint32_t m_DescriptorSize = 0;
		uint32_t m_CurrentOffset = 0;
		std::vector<win32::com_ptr<ID3D12DescriptorHeap>> m_RetiredHeaps;

		struct FDescriptorTableCache
		{
			FDescriptorTableCache()
				: AssignedHandlesBitMap(0)
				, TableSize(0)
				, TableStart(nullptr) {}

			uint32_t AssignedHandlesBitMap;
			uint32_t TableSize;
			D3D12_CPU_DESCRIPTOR_HANDLE* TableStart;
		};

		struct FDescriptorHandleCache
		{
			FDescriptorHandleCache()
			{
				ClearCache();
			}

			void ClearCache()
			{
				m_StaleRootParamsBitMap = 0;
				m_RootDescriptorTablesBitMap = 0;
				m_MaxCachedDescriptors = 0;
				ZeroMemory(m_HandleCache, sizeof(m_HandleCache));
				ZeroMemory(m_RootDescriptorTable, sizeof(m_RootDescriptorTable));
			}

			void UnbindAllInvalid();

			uint32_t ComputeStagedSize();
			void ParseRootSignature(D3D12_DESCRIPTOR_HEAP_TYPE Type, const FRootSignature& RootSignature);
			void StageDescriptorHandles(UINT RootIndex, UINT Offset, UINT Count, const D3D12_CPU_DESCRIPTOR_HANDLE Handles[]);
			void CopyAndBindStaleTables(D3D12_DESCRIPTOR_HEAP_TYPE Type, ID3D12Device* InDevice, uint32_t DescriptorSize, FDescriptorHandle DestHandleStart, ID3D12GraphicsCommandList* CmdList,
				void (STDMETHODCALLTYPE ID3D12GraphicsCommandList::* SetFunc)(UINT, D3D12_GPU_DESCRIPTOR_HANDLE));

			uint32_t m_RootDescriptorTablesBitMap = 0;
			uint32_t m_StaleRootParamsBitMap = 0;
			uint32_t m_MaxCachedDescriptors = 0;

			static const uint32_t MaxNumDescriptors = 4096;
			static const uint32_t MaxNumDescriptorTables = 16;
			FDescriptorTableCache m_RootDescriptorTable[MaxNumDescriptorTables] = {};
			D3D12_CPU_DESCRIPTOR_HANDLE m_HandleCache[MaxNumDescriptors] = {};
		};
		FDescriptorHandleCache m_GraphicsHandleCache;
		FDescriptorHandleCache m_ComputeHandleCache;

		FDescriptorHandle AllocateDescriptor(UINT Count)
		{
			Assert(m_CurrentHeap != nullptr);
			FDescriptorHandle Result = m_FirstDescriptor + m_CurrentOffset * m_DescriptorSize;
			m_CurrentOffset += Count;
			return Result;
		}

		void CopyAndBindStagedTables(FDescriptorHandleCache& HandleCache, ID3D12Device*InDevice,ID3D12GraphicsCommandList* CommandList,
			void(STDMETHODCALLTYPE ID3D12GraphicsCommandList::* SetFunc)(UINT, D3D12_GPU_DESCRIPTOR_HANDLE));
	};
}