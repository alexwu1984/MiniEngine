#include "D3D12/D3D12DescriptorCache.h"
#include "D3D12/D3D12WindowDevice.h"
#include "D3D12/D3D12DirectCommandListManager.h"
#include "D3D12/D3D12CommandContext.h"
#include "D3D12/D3D12RootSignature.h"

namespace RenderCore
{
	std::queue<win32::com_ptr<ID3D12DescriptorHeap>> FDynamicDescriptorHeap::ms_ReadyDescriptorHeaps[2];
	std::queue<std::pair<uint64_t, win32::com_ptr<ID3D12DescriptorHeap>>> FDynamicDescriptorHeap::ms_RetiredDescriptorHeaps[2];
	std::vector<win32::com_ptr<ID3D12DescriptorHeap>> FDynamicDescriptorHeap::ms_DescriptorHeapPool[2];

	FDynamicDescriptorHeap::FDynamicDescriptorHeap(std::weak_ptr<FD3D12Device> InDevice,
												   std::weak_ptr<D3D12CommandContext> CommandContext,
											       D3D12_DESCRIPTOR_HEAP_TYPE HeapType)
		:FD3D12DeviceChild(InDevice),
		m_HeapType(HeapType),
		m_OwningContext(CommandContext.lock())
	{
		m_DescriptorSize = GetParentDevice()->GetDevice()->GetDescriptorHandleIncrementSize(HeapType);
	}

	void FDynamicDescriptorHeap::CommitGraphicsRootDescriptorTables(ID3D12GraphicsCommandList* CommandList)
	{
		if (m_GraphicsHandleCache.m_StaleRootParamsBitMap)
		{
			CopyAndBindStagedTables(m_GraphicsHandleCache, GetParentDevice()->GetDevice(),
				CommandList, &ID3D12GraphicsCommandList::SetGraphicsRootDescriptorTable);
		}
	}

	void FDynamicDescriptorHeap::CommitComputeRootDescriptorTables(ID3D12GraphicsCommandList* CommandList)
	{
		if (m_ComputeHandleCache.m_StaleRootParamsBitMap)
		{
			CopyAndBindStagedTables(m_ComputeHandleCache, GetParentDevice()->GetDevice(),
				CommandList, &ID3D12GraphicsCommandList::SetComputeRootDescriptorTable);
		}
	}

	void FDynamicDescriptorHeap::CleanupUsedHeaps(uint64_t FenceValue)
	{
		RetireCurrentHeap();
		RetireUsedHeaps(FenceValue);
		m_GraphicsHandleCache.ClearCache();
		m_ComputeHandleCache.ClearCache();
	}

	void FDynamicDescriptorHeap::RetireCurrentHeap()
	{
		if (m_CurrentOffset == 0)
		{
			Assert(m_CurrentHeap == nullptr);
			return;
		}

		Assert(m_CurrentHeap != nullptr);
		m_RetiredHeaps.push_back(m_CurrentHeap);
		m_CurrentOffset = 0;
		m_CurrentHeap = nullptr;
	}

	void FDynamicDescriptorHeap::RetireUsedHeaps(uint64_t FenceValue)
	{
		uint32_t idx = m_HeapType == D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER ? 1 : 0;
		for (auto iter = m_RetiredHeaps.begin(); iter != m_RetiredHeaps.end(); ++iter)
			ms_RetiredDescriptorHeaps[idx].push(std::make_pair(FenceValue, *iter));
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
		auto& CommandListMgr = GetParentDevice()->GetCommandListManager();
		while (!ms_RetiredDescriptorHeaps[idx].empty() && CommandListMgr.GetFence().IsFenceComplete(ms_RetiredDescriptorHeaps[idx].front().first))
		{
			ms_ReadyDescriptorHeaps[idx].push(ms_RetiredDescriptorHeaps[idx].front().second);
			ms_RetiredDescriptorHeaps[idx].pop();
		}
		if (!ms_ReadyDescriptorHeaps[idx].empty())
		{
			win32::com_ptr<ID3D12DescriptorHeap> Heap = ms_ReadyDescriptorHeaps[idx].front();
			ms_ReadyDescriptorHeaps[idx].pop();
			return Heap;
		}
		else
		{
			D3D12_DESCRIPTOR_HEAP_DESC HeapDesc = {};
			HeapDesc.Type = HeapType;
			HeapDesc.NumDescriptors = NumDescriptorsPerHeap;
			HeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
			HeapDesc.NodeMask = 1;
			win32::com_ptr<ID3D12DescriptorHeap> Heap;
			VERIFYD3DRESULT(GetParentDevice()->GetDevice()->CreateDescriptorHeap(&HeapDesc, IID_PPV_ARGS(&Heap)));
			ms_DescriptorHeapPool[idx].emplace_back(Heap);
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
		Device->CopyDescriptorsSimple(1, DestHandle.GetCpuHandle(), Handle, m_HeapType);

		return DestHandle.GetGpuHandle();
	}

	void FDynamicDescriptorHeap::CopyAndBindStagedTables(FDescriptorHandleCache& HandleCache, ID3D12Device* InDevice, ID3D12GraphicsCommandList* CommandList,
		void(STDMETHODCALLTYPE ID3D12GraphicsCommandList::* SetFunc)(UINT, D3D12_GPU_DESCRIPTOR_HANDLE))
	{
		uint32_t NeededSize = HandleCache.ComputeStagedSize();
		if (!HasSpace(NeededSize))
		{
			RetireCurrentHeap();
			UnbindAllInvalid();
			NeededSize = HandleCache.ComputeStagedSize();
		}

		m_OwningContext->SetDescriptorHeap(m_HeapType, GetHeapPointer());
		HandleCache.CopyAndBindStaleTables(m_HeapType,InDevice, m_DescriptorSize, AllocateDescriptor(NeededSize), CommandList, SetFunc);
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

			DWORD MaxSetHandle;
			BOOL Result = _BitScanReverse(&MaxSetHandle, m_RootDescriptorTable[RootIndex].AssignedHandlesBitMap);
			Assert(Result);

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
		static const uint32_t kMaxDescriptorsPerCopy = 16;
		UINT NumDstDescriptorRanges = 0;
		D3D12_CPU_DESCRIPTOR_HANDLE pDstDescriptorRangeStarts[kMaxDescriptorsPerCopy];
		UINT pDstDescriptorRangeSizes[kMaxDescriptorsPerCopy];

		UINT NumSrcDescriptorRanges = 0;
		D3D12_CPU_DESCRIPTOR_HANDLE pSrcDescriptorRangeStarts[kMaxDescriptorsPerCopy];
		UINT pSrcDescriptorRangeSizes[kMaxDescriptorsPerCopy];

		DWORD RootIndex;
		DWORD StaleParams = m_StaleRootParamsBitMap;
		while (_BitScanForward(&RootIndex, StaleParams))
		{
			StaleParams ^= (1 << RootIndex);

			DWORD TableSize = 0;
			BOOL Result = _BitScanReverse(&TableSize, m_RootDescriptorTable[RootIndex].AssignedHandlesBitMap);
			Assert(Result);
			TableSize += 1;

			(CmdList->*SetFunc)(RootIndex, DestHandleStart.GetGpuHandle());

			FDescriptorTableCache& TableCache = m_RootDescriptorTable[RootIndex];
			D3D12_CPU_DESCRIPTOR_HANDLE* SrcHandles = TableCache.TableStart;
			uint32_t SetHandles = TableCache.AssignedHandlesBitMap;
			D3D12_CPU_DESCRIPTOR_HANDLE CurDest = DestHandleStart.GetCpuHandle();
			DestHandleStart += TableSize * DescriptorSize;

			DWORD SkipCount;
			while (_BitScanForward(&SkipCount, SetHandles))
			{
				SetHandles >>= SkipCount;
				SrcHandles += SkipCount;
				CurDest.ptr += SkipCount * DescriptorSize;

				DWORD DescriptorCount;
				_BitScanForward(&DescriptorCount, ~SetHandles);
				SetHandles >>= DescriptorCount;

				if (NumSrcDescriptorRanges + DescriptorCount > kMaxDescriptorsPerCopy)
				{
					InDevice->CopyDescriptors(
						NumDstDescriptorRanges, pDstDescriptorRangeStarts, pDstDescriptorRangeSizes,
						NumSrcDescriptorRanges, pSrcDescriptorRangeStarts, pSrcDescriptorRangeSizes,
						Type);
					NumSrcDescriptorRanges = 0;
					NumDstDescriptorRanges = 0;
				}

				pDstDescriptorRangeStarts[NumDstDescriptorRanges] = CurDest;
				pDstDescriptorRangeSizes[NumDstDescriptorRanges] = DescriptorCount;
				++NumDstDescriptorRanges;

				for (uint32_t i = 0; i < DescriptorCount; ++i)
				{
					pSrcDescriptorRangeStarts[NumSrcDescriptorRanges] = SrcHandles[i];
					pSrcDescriptorRangeSizes[NumSrcDescriptorRanges] = 1;
					++NumSrcDescriptorRanges;
				}

				SrcHandles += DescriptorCount;
				CurDest.ptr += DescriptorCount * DescriptorSize;
			}
		}

		InDevice->CopyDescriptors(
			NumDstDescriptorRanges, pDstDescriptorRangeStarts, pDstDescriptorRangeSizes,
			NumSrcDescriptorRanges, pSrcDescriptorRangeStarts, pSrcDescriptorRangeSizes,
			Type);
	}

}