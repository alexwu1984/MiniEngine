#include "D3D12/D3D12DescriptorCache.h"
#include "RHI/RHI.h"
#include "D3D12/D3D12WindowDevice.h"
#include "D3D12/D3D12Adapter.h"
#include "D3D12/D3D12DirectCommandListManager.h"
#include "D3D12/D3D12CommandContext.h"
#include "D3D12/D3D12RootSignature.h"
#include "D3D12/D3D12CreateStats.h"

namespace RenderCore
{
	bool FDescriptorHandle::IsValidShaderVisibleTableBase() const
	{
		const SIZE_T cpu = m_CpuHandle.ptr;
		const UINT64 gpu = m_GpuHandle.ptr;
		return cpu != 0 && cpu != D3D12_CPU_VIRTUAL_ADDRESS_UNKNOWN
			&& gpu != 0 && gpu != D3D12_GPU_VIRTUAL_ADDRESS_UNKNOWN;
	}

	void FDynamicDescriptorHeapPoolsPerDevice::Clear()
	{
		for (int i = 0; i < 2; ++i)
		{
			while (!Ready[i].empty())
				Ready[i].pop();
			for (int q = 0; q < 3; ++q)
			{
				while (!Retired[i][q].empty())
					Retired[i][q].pop();
			}
			CreatedTracking[i].clear();
		}
	}

	namespace
	{
		static int QueueTypeIndex(ED3D12CommandQueueType Q)
		{
			switch (Q)
			{
			case ED3D12CommandQueueType::Default: return 0;
			case ED3D12CommandQueueType::Copy:    return 1;
			case ED3D12CommandQueueType::Async:   return 2;
			default: return 0;
			}
		}
	}

	FDynamicDescriptorHeapPoolsPerDevice& FDynamicDescriptorHeap::Pools()
	{
		return GetParentDevice()->GetDynamicDescriptorHeapPools();
	}

	FDynamicDescriptorHeap::FDynamicDescriptorHeap(std::weak_ptr<FD3D12Device> InDevice,
												   std::weak_ptr<D3D12CommandContext> CommandContext,
											       D3D12_DESCRIPTOR_HEAP_TYPE HeapType)
		:FD3D12DeviceChild(InDevice),
		m_HeapType(HeapType),
		m_OwningContext(CommandContext.lock())
	{
		m_DescriptorSize = GetParentDevice()->GetDevice()->GetDescriptorHandleIncrementSize(HeapType);
		// Sampler shader-visible heaps: stay conservative (table limits differ from CBV/SRV/UAV).
		if (HeapType == D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER)
		{
			m_NumDescriptorsPerHeap = 2048u;
		}
		else
		{
			const D3D12_RESOURCE_BINDING_TIER Tier = GetParentDevice()->GetParentAdapter()->GetResourceBindingTier();
			if (Tier >= D3D12_RESOURCE_BINDING_TIER_3)
				m_NumDescriptorsPerHeap = 65536u;
			else if (Tier == D3D12_RESOURCE_BINDING_TIER_2)
				m_NumDescriptorsPerHeap = 32768u;
			else
				m_NumDescriptorsPerHeap = 16384u;
		}
	}

	void FDynamicDescriptorHeap::CommitGraphicsRootDescriptorTables(ID3D12GraphicsCommandList* CommandList)
	{
		if (!CommandList)
			return;
		if (m_GraphicsHandleCache.m_StaleRootParamsBitMap)
		{
			CopyAndBindStagedTables(m_GraphicsHandleCache, GetParentDevice()->GetDevice(),
				CommandList, &ID3D12GraphicsCommandList::SetGraphicsRootDescriptorTable);
		}
	}

	void FDynamicDescriptorHeap::CommitComputeRootDescriptorTables(ID3D12GraphicsCommandList* CommandList)
	{
		if (!CommandList)
			return;
		if (m_ComputeHandleCache.m_StaleRootParamsBitMap)
		{
			CopyAndBindStagedTables(m_ComputeHandleCache, GetParentDevice()->GetDevice(),
				CommandList, &ID3D12GraphicsCommandList::SetComputeRootDescriptorTable);
		}
	}

	void FDynamicDescriptorHeap::CleanupUsedHeaps(uint64_t FenceValue, ED3D12CommandQueueType QueueType)
	{
		RetireCurrentHeap();
		RetireUsedHeaps(FenceValue, QueueType);
		m_GraphicsHandleCache.ClearCache();
		m_ComputeHandleCache.ClearCache();
	}

	void FDynamicDescriptorHeap::RetireCurrentHeap()
	{
		if (m_CurrentHeap == nullptr)
			return;

		m_RetiredHeaps.push_back(m_CurrentHeap);
		m_CurrentHeap = nullptr;
		m_CurrentOffset = 0;
	}

	void FDynamicDescriptorHeap::RetireUsedHeaps(uint64_t FenceValue, ED3D12CommandQueueType QueueType)
	{
		uint32_t idx = m_HeapType == D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER ? 1 : 0;
		const int qidx = QueueTypeIndex(QueueType);
		for (auto iter = m_RetiredHeaps.begin(); iter != m_RetiredHeaps.end(); ++iter)
		{
			FRetiredDynamicDescriptorHeapEntry Entry;
			Entry.FenceValue = FenceValue;
			Entry.QueueType = QueueType;
			Entry.Heap = *iter;
			Pools().Retired[idx][qidx].push(Entry);
		}
		m_RetiredHeaps.clear();
	}

	void FDynamicDescriptorHeap::UnbindAllInvalid()
	{
		m_GraphicsHandleCache.UnbindAllInvalid();
		m_ComputeHandleCache.UnbindAllInvalid();
	}

	win32::com_ptr<ID3D12DescriptorHeap> FDynamicDescriptorHeap::GetHeapPointer()
	{
		if (m_CurrentHeap == nullptr)
		{
			Assert(m_CurrentOffset == 0);
			m_CurrentHeap = RequestDescriptorHeap(m_HeapType);
			m_FirstDescriptor = FDescriptorHandle(m_CurrentHeap.get());
		}
		return m_CurrentHeap;
	}

	win32::com_ptr<ID3D12DescriptorHeap> FDynamicDescriptorHeap::RequestDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE HeapType)
	{
		uint32_t idx = m_HeapType == D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER ? 1 : 0;
		std::shared_ptr<FD3D12Device> Device = GetParentDevice();
		FDynamicDescriptorHeapPoolsPerDevice& P = Pools();
		// Recycle heaps using monotonic per-queue retired lists.
		for (int q = 0; q < 3; ++q)
		{
			while (!P.Retired[idx][q].empty())
			{
				const FRetiredDynamicDescriptorHeapEntry& Entry = P.Retired[idx][q].front();
				FD3D12CommandListManager& Mgr = Device->GetCommandListManager(Entry.QueueType);
				if (!Mgr.GetFence().IsFenceComplete(Entry.FenceValue))
					break;
				P.Ready[idx].push(Entry.Heap);
				P.Retired[idx][q].pop();
			}
		}
		if (!P.Ready[idx].empty())
		{
			win32::com_ptr<ID3D12DescriptorHeap> Heap = P.Ready[idx].front();
			P.Ready[idx].pop();
			if (idx == 0)
				D3D12MemMonAtomicAdd(D3D12CreateStats::DynDesc_RecycleReadyCount_CbvSrvUav());
			else
				D3D12MemMonAtomicAdd(D3D12CreateStats::DynDesc_RecycleReadyCount_Sampler());
			return Heap;
		}
		else
		{
			// Prevent unbounded growth when the GPU falls behind (common without the debug layer).
			// If we have created "too many" shader-visible heaps and none are ready, wait for the
			// oldest retired heap to complete and reuse it instead of creating more.
			static constexpr size_t kMaxShaderVisibleHeapsPerType = 256;
			const bool bHasAnyRetired = (!P.Retired[idx][0].empty() || !P.Retired[idx][1].empty() || !P.Retired[idx][2].empty());
			if (P.CreatedTracking[idx].size() >= kMaxShaderVisibleHeapsPerType && bHasAnyRetired)
			{
				// Pick a queue to wait on (prefer Default queue).
				int PickQ = !P.Retired[idx][0].empty() ? 0 : (!P.Retired[idx][1].empty() ? 1 : 2);
				FRetiredDynamicDescriptorHeapEntry Oldest = P.Retired[idx][PickQ].front();
				P.Retired[idx][PickQ].pop();

				FD3D12CommandListManager& Mgr = Device->GetCommandListManager(Oldest.QueueType);
				Mgr.GetFence().WaitForFence(Oldest.FenceValue);
				if (idx == 0)
					D3D12MemMonAtomicAdd(D3D12CreateStats::DynDesc_FenceWaitReuseCount_CbvSrvUav());
				else
					D3D12MemMonAtomicAdd(D3D12CreateStats::DynDesc_FenceWaitReuseCount_Sampler());
				P.Ready[idx].push(Oldest.Heap);

				win32::com_ptr<ID3D12DescriptorHeap> Heap = P.Ready[idx].front();
				P.Ready[idx].pop();
				return Heap;
			}

			D3D12_DESCRIPTOR_HEAP_DESC HeapDesc = {};
			HeapDesc.Type = HeapType;
			HeapDesc.NumDescriptors = m_NumDescriptorsPerHeap;
			HeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
			HeapDesc.NodeMask = 1;
			win32::com_ptr<ID3D12DescriptorHeap> Heap;
			VERIFYD3DRESULT(GetParentDevice()->GetDevice()->CreateDescriptorHeap(&HeapDesc, IID_PPV_ARGS(&Heap)));
			P.CreatedTracking[idx].emplace_back(Heap);
			if (idx == 0)
				D3D12MemMonAtomicAdd(D3D12CreateStats::DynDesc_CreateCount_CbvSrvUav());
			else
				D3D12MemMonAtomicAdd(D3D12CreateStats::DynDesc_CreateCount_Sampler());
			return Heap;
		}
	}

	D3D12_GPU_DESCRIPTOR_HANDLE FDynamicDescriptorHeap::UploadDirect(D3D12_CPU_DESCRIPTOR_HANDLE Handle)
	{
		if (!HasSpace(1))
		{
			RetireCurrentHeap();
			UnbindAllInvalid();
		}

		m_OwningContext->SetDescriptorHeap(m_HeapType, GetHeapPointer());

		FDescriptorHandle DestHandle = m_FirstDescriptor + m_CurrentOffset * m_DescriptorSize;
		m_CurrentOffset += 1;

		ID3D12Device* Device = GetParentDevice()->GetDevice();
		if (m_HeapType == D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER)
		{
			D3D12MemMonAtomicAdd(D3D12CreateStats::DynDesc_CopyDescriptorsCalls_Sampler());
			D3D12MemMonAtomicAdd(D3D12CreateStats::DynDesc_CopyDescriptorsCount_Sampler());
		}
		else
		{
			D3D12MemMonAtomicAdd(D3D12CreateStats::DynDesc_CopyDescriptorsCalls_CbvSrvUav());
			D3D12MemMonAtomicAdd(D3D12CreateStats::DynDesc_CopyDescriptorsCount_CbvSrvUav());
		}
		Device->CopyDescriptorsSimple(1, DestHandle.GetCpuHandle(), Handle, m_HeapType);

		return DestHandle.GetGpuHandle();
	}

	void FDynamicDescriptorHeap::CopyAndBindStagedTables(FDescriptorHandleCache& HandleCache, ID3D12Device* InDevice, ID3D12GraphicsCommandList* CommandList,
		void(STDMETHODCALLTYPE ID3D12GraphicsCommandList::* SetFunc)(UINT, D3D12_GPU_DESCRIPTOR_HANDLE))
	{
		uint32_t NeededSize = HandleCache.ComputeStagedSize();
		if (NeededSize == 0)
		{
			// Stale bits without valid staged handles (e.g. cache cleared mid-frame) — do not call SetGraphicsRootDescriptorTable with a bad range.
			HandleCache.m_StaleRootParamsBitMap = 0;
			return;
		}
		if (NeededSize > 0)
		{
			if (m_HeapType == D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER)
			{
				D3D12MemMonAtomicAdd(D3D12CreateStats::DynDesc_CopyDescriptorsCalls_Sampler());
				D3D12MemMonAtomicAdd(D3D12CreateStats::DynDesc_CopyDescriptorsCount_Sampler(), NeededSize);
			}
			else
			{
				D3D12MemMonAtomicAdd(D3D12CreateStats::DynDesc_CopyDescriptorsCalls_CbvSrvUav());
				D3D12MemMonAtomicAdd(D3D12CreateStats::DynDesc_CopyDescriptorsCount_CbvSrvUav(), NeededSize);
			}
		}
		if (!HasSpace(NeededSize))
		{
			RetireCurrentHeap();
			UnbindAllInvalid();
			NeededSize = HandleCache.ComputeStagedSize();
		}
		if (NeededSize == 0)
		{
			HandleCache.m_StaleRootParamsBitMap = 0;
			return;
		}

		if (!m_OwningContext)
		{
			HandleCache.m_StaleRootParamsBitMap = 0;
			return;
		}

		m_OwningContext->SetDescriptorHeap(m_HeapType, GetHeapPointer());
		// One commit must fit in the current shader-visible heap chunk; otherwise AllocateDescriptor walks past
		// heap end and CopyAndBindStaleTables / the driver faults (e.g. AV reading ~0x18 in nvwgf2umx).
		if (!HasSpace(NeededSize))
		{
			HandleCache.m_StaleRootParamsBitMap = 0;
			return;
		}

		const FDescriptorHandle Allocated = AllocateDescriptor(NeededSize);
		if (!Allocated.IsValidShaderVisibleTableBase())
		{
			HandleCache.m_StaleRootParamsBitMap = 0;
			return;
		}
		HandleCache.CopyAndBindStaleTables(m_HeapType, InDevice, m_DescriptorSize, Allocated, CommandList, SetFunc);
	}

	void FDynamicDescriptorHeap::FDescriptorHandleCache::UnbindAllInvalid()
	{
		m_StaleRootParamsBitMap = 0;
		DWORD TableParams = m_RootDescriptorTablesBitMap;
		DWORD RootIndex;
		while (_BitScanForward(&RootIndex, TableParams))
		{
			TableParams ^= (1 << RootIndex);
			if (m_RootDescriptorTable[RootIndex].AssignedHandlesBitMap != 0)
				m_StaleRootParamsBitMap |= (1 << RootIndex);
		}
	}

	uint32_t FDynamicDescriptorHeap::FDescriptorHandleCache::ComputeStagedSize()
	{
		uint32_t NeededSpace = 0;
		DWORD RootIndex;
		DWORD StaleParams = m_StaleRootParamsBitMap;
		while (_BitScanForward(&RootIndex, StaleParams))
		{
			StaleParams ^= (1 << RootIndex);
			const FDescriptorTableCache& Table = m_RootDescriptorTable[RootIndex];
			if (Table.TableStart == nullptr || Table.AssignedHandlesBitMap == 0)
				continue;

			DWORD MaxSetHandle = 0;
			const BOOL Result = _BitScanReverse(&MaxSetHandle, Table.AssignedHandlesBitMap);
			if (!Result)
				continue;

			NeededSpace += MaxSetHandle + 1;
		}
		return NeededSpace;
	}

	void FDynamicDescriptorHeap::FDescriptorHandleCache::ParseRootSignature(D3D12_DESCRIPTOR_HEAP_TYPE Type, const FRootSignature& RootSignature)
	{
		Assert(RootSignature.GetNumParameters() <= 16);

		UINT CurrentOffset = 0;
		m_StaleRootParamsBitMap = 0;
		m_RootDescriptorTablesBitMap = (Type == D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER ? RootSignature.GetSamplerTableBitMap() : RootSignature.GetDescriptorTableBitMap());

		DWORD RootIndex;
		DWORD TableParams = m_RootDescriptorTablesBitMap;
		while (_BitScanForward(&RootIndex, TableParams))
		{
			TableParams ^= (1 << RootIndex);
			UINT TableSize = RootSignature.GetDescriptorTableSize(RootIndex);
			Assert(TableSize > 0);

			FDescriptorTableCache& DescriptorTable = m_RootDescriptorTable[RootIndex];
			DescriptorTable.AssignedHandlesBitMap = 0;
			DescriptorTable.TableStart = m_HandleCache + CurrentOffset;
			DescriptorTable.TableSize = TableSize;

			CurrentOffset += TableSize;
		}
		m_MaxCachedDescriptors = CurrentOffset;

		Assert(m_MaxCachedDescriptors <= MaxNumDescriptors);
	}

	void FDynamicDescriptorHeap::FDescriptorHandleCache::StageDescriptorHandles(UINT RootIndex, UINT Offset, UINT Count, const D3D12_CPU_DESCRIPTOR_HANDLE Handles[])
	{
		Assert(((1 << RootIndex) & m_RootDescriptorTablesBitMap) != 0);
		Assert(Offset + Count <= m_RootDescriptorTable[RootIndex].TableSize);

		FDescriptorTableCache& TableCache = m_RootDescriptorTable[RootIndex];
		D3D12_CPU_DESCRIPTOR_HANDLE* CopyDest = TableCache.TableStart + Offset;
		for (UINT i = 0; i < Count; ++i)
			CopyDest[i] = Handles[i];

		TableCache.AssignedHandlesBitMap |= (((1 << Count) - 1) << Offset);
		m_StaleRootParamsBitMap |= (1 << RootIndex);
	}

	void FDynamicDescriptorHeap::FDescriptorHandleCache::CopyAndBindStaleTables(
		D3D12_DESCRIPTOR_HEAP_TYPE Type, ID3D12Device* InDevice,uint32_t DescriptorSize,
		FDescriptorHandle DestHandleStart,
		ID3D12GraphicsCommandList* CmdList,
		void (STDMETHODCALLTYPE ID3D12GraphicsCommandList::* SetFunc)(UINT, D3D12_GPU_DESCRIPTOR_HANDLE))
	{
		if (!InDevice || !CmdList || !DestHandleStart.IsValidShaderVisibleTableBase())
		{
			m_StaleRootParamsBitMap = 0;
			return;
		}

		// Range count limit for CopyDescriptors (dst+src entries). Large enough to avoid splitting one bind table.
		static const uint32_t kMaxRanges = 1024;

		UINT NumDstDescriptorRanges = 0;
		D3D12_CPU_DESCRIPTOR_HANDLE pDstDescriptorRangeStarts[kMaxRanges];
		UINT pDstDescriptorRangeSizes[kMaxRanges];

		UINT NumSrcDescriptorRanges = 0;
		D3D12_CPU_DESCRIPTOR_HANDLE pSrcDescriptorRangeStarts[kMaxRanges];
		UINT pSrcDescriptorRangeSizes[kMaxRanges];

		auto flushBatch = [&]()
		{
			if (NumDstDescriptorRanges == 0)
				return;
			InDevice->CopyDescriptors(
				NumDstDescriptorRanges, pDstDescriptorRangeStarts, pDstDescriptorRangeSizes,
				NumSrcDescriptorRanges, pSrcDescriptorRangeStarts, pSrcDescriptorRangeSizes,
				Type);
			NumDstDescriptorRanges = 0;
			NumSrcDescriptorRanges = 0;
		};

		DWORD RootIndex;
		DWORD StaleParams = m_StaleRootParamsBitMap;
		while (_BitScanForward(&RootIndex, StaleParams))
		{
			StaleParams ^= (1 << RootIndex);

			FDescriptorTableCache& TableCache = m_RootDescriptorTable[RootIndex];
			if (TableCache.TableStart == nullptr || TableCache.AssignedHandlesBitMap == 0)
			{
				m_StaleRootParamsBitMap &= ~(1u << RootIndex);
				continue;
			}

			DWORD TableSize = 0;
			const BOOL Result = _BitScanReverse(&TableSize, TableCache.AssignedHandlesBitMap);
			if (!Result)
			{
				m_StaleRootParamsBitMap &= ~(1u << RootIndex);
				continue;
			}
			TableSize += 1;

			const D3D12_GPU_DESCRIPTOR_HANDLE TableBindGpu = DestHandleStart.GetGpuHandle();
			D3D12_CPU_DESCRIPTOR_HANDLE* SrcHandles = TableCache.TableStart;
			uint32_t SetHandles = TableCache.AssignedHandlesBitMap;
			D3D12_CPU_DESCRIPTOR_HANDLE CurDest = DestHandleStart.GetCpuHandle();
			DestHandleStart += static_cast<INT>(TableSize * DescriptorSize);

			DWORD SkipCount;
			while (_BitScanForward(&SkipCount, SetHandles))
			{
				SetHandles >>= SkipCount;
				SrcHandles += SkipCount;
				CurDest.ptr += SkipCount * DescriptorSize;

				DWORD DescriptorCount;
				_BitScanForward(&DescriptorCount, ~SetHandles);
				SetHandles >>= DescriptorCount;

				// Count merged source ranges (adjacent CPU heap slots share one CopyDescriptors src range).
				uint32_t mergedRuns = 0;
				for (uint32_t j = 0; j < DescriptorCount; )
				{
					++mergedRuns;
					uint32_t runLen = 1;
					while (j + runLen < DescriptorCount &&
						   SrcHandles[j + runLen].ptr == SrcHandles[j + runLen - 1].ptr + DescriptorSize)
						++runLen;
					j += runLen;
				}

				if (NumDstDescriptorRanges + 1 > kMaxRanges || NumSrcDescriptorRanges + mergedRuns > kMaxRanges)
					flushBatch();

				pDstDescriptorRangeStarts[NumDstDescriptorRanges] = CurDest;
				pDstDescriptorRangeSizes[NumDstDescriptorRanges] = DescriptorCount;
				++NumDstDescriptorRanges;

				for (uint32_t j = 0; j < DescriptorCount; )
				{
					uint32_t runLen = 1;
					while (j + runLen < DescriptorCount &&
						   SrcHandles[j + runLen].ptr == SrcHandles[j + runLen - 1].ptr + DescriptorSize)
						++runLen;

					pSrcDescriptorRangeStarts[NumSrcDescriptorRanges] = SrcHandles[j];
					pSrcDescriptorRangeSizes[NumSrcDescriptorRanges] = runLen;
					++NumSrcDescriptorRanges;
					j += runLen;
				}

				SrcHandles += DescriptorCount;
				CurDest.ptr += DescriptorCount * DescriptorSize;
			}

			flushBatch();
			(CmdList->*SetFunc)(RootIndex, TableBindGpu);
		}

		flushBatch();
		// Tables are now uploaded; avoid re-copy on the next bind unless Stage* marks stale again.
		m_StaleRootParamsBitMap = 0;
	}

}