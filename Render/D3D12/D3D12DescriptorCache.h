#pragma once
#include "RHIPrivate/D3D12RHIPrivate.h"
#include "Templates/UnrealTypeTraits.h"
#include "D3D12/MultiGPU.h"
#include "D3D12/D3D12CommandList.h"

namespace RenderCore
{
	struct FD3D12SamplerArrayDesc
	{
		uint32_t Count;
		uint16_t SamplerID[16];
		inline bool operator==(const FD3D12SamplerArrayDesc& rhs) const
		{
			assert(Count <= _ARRAYSIZE(SamplerID));
			assert(rhs.Count <= _ARRAYSIZE(rhs.SamplerID));

			if (Count != rhs.Count)
			{
				return false;
			}
			else
			{
				// It is safe to compare pointers, because samplers are kept alive for the lifetime of the RHI
				return 0 == memcmp(SamplerID, rhs.SamplerID, sizeof(SamplerID[0]) * Count);
			}
		}
	};
	uint32_t GetTypeHash(const FD3D12SamplerArrayDesc& Key);

	template< uint32_t CPUTableSize>
	struct FD3D12UniqueDescriptorTable
	{
		FD3D12UniqueDescriptorTable() : GPUHandle({}) {};
		FD3D12UniqueDescriptorTable(FD3D12SamplerArrayDesc KeyIn, CD3DX12_CPU_DESCRIPTOR_HANDLE* Table) : GPUHandle({})
		{
			memcpy(&Key, &KeyIn, sizeof(Key));//Memcpy to avoid alignement issues
			memcpy(CPUTable, Table, Key.Count * sizeof(CD3DX12_CPU_DESCRIPTOR_HANDLE));
		}

		FORCEINLINE uint32_t GetTypeHash(const FD3D12UniqueDescriptorTable& Table)
		{
			return SSE4_CRC32((void*)Table.Key.SamplerID, Table.Key.Count * sizeof(Table.Key.SamplerID[0]));
		}

		FD3D12SamplerArrayDesc Key;
		CD3DX12_CPU_DESCRIPTOR_HANDLE CPUTable[MAX_SAMPLERS];

		// This will point to the table start in the global heap
		D3D12_GPU_DESCRIPTOR_HANDLE GPUHandle;
	};

	template<typename FD3D12UniqueDescriptorTable, bool bInAllowDuplicateKeys = false>
	struct FD3D12UniqueDescriptorTableKeyFuncs /*: BaseKeyFuncs<FD3D12UniqueDescriptorTable, FD3D12UniqueDescriptorTable, bInAllowDuplicateKeys>*/
	{
		typedef typename TCallTraits<FD3D12UniqueDescriptorTable>::ParamType KeyInitType;
		typedef typename TCallTraits<FD3D12UniqueDescriptorTable>::ParamType ElementInitType;

		/**
		* @return The key used to index the given element.
		*/
		static FORCEINLINE KeyInitType GetSetKey(ElementInitType Element)
		{
			return Element;
		}

		/**
		* @return True if the keys match.
		*/
		static FORCEINLINE bool Matches(KeyInitType A, KeyInitType B)
		{
			return A.Key == B.Key;
		}

		/** Calculates a hash index for a key. */
		static FORCEINLINE uint32_t GetKeyHash(KeyInitType Key)
		{
			return GetTypeHash(Key.Key);
		}

		constexpr bool operator()(const KeyInitType& _Left, const KeyInitType& _Right) const
		{	// apply operator< to operands
			//return (_Left < _Right);
			uint32_t leftValue = GetKeyHash(_Left);
			uint32_t rightValue = GetKeyHash(_Right);

			return leftValue < rightValue;
		}
	};

	typedef FD3D12UniqueDescriptorTable<MAX_SAMPLERS> FD3D12UniqueSamplerTable;
	typedef std::set<FD3D12UniqueSamplerTable, FD3D12UniqueDescriptorTableKeyFuncs<FD3D12UniqueSamplerTable>> FD3D12SamplerSet;
	class FD3D12DescriptorCache;

	class FD3D12OfflineDescriptorManager 
	{
	public: // Types
		typedef D3D12_CPU_DESCRIPTOR_HANDLE HeapOffset;
		typedef decltype(HeapOffset::ptr) HeapOffsetRaw;
		typedef uint32_t HeapIndex;

	private: // Types
		struct SFreeRange { HeapOffsetRaw Start; HeapOffsetRaw End; };
		struct SHeapEntry
		{
			win32::com_ptr<ID3D12DescriptorHeap> m_Heap;
			std::list<SFreeRange> m_FreeList;

			SHeapEntry() { }
		};
		typedef std::vector<SHeapEntry> THeapMap;

		static D3D12_DESCRIPTOR_HEAP_DESC CreateDescriptor(FRHIGPUMask Node, D3D12_DESCRIPTOR_HEAP_TYPE Type, uint32_t NumDescriptorsPerHeap)
		{
			D3D12_DESCRIPTOR_HEAP_DESC Desc = {};
			Desc.Type = Type;
			Desc.NumDescriptors = NumDescriptorsPerHeap;
			Desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;// None as this heap is offline
			Desc.NodeMask = (uint32_t)Node;

			return Desc;
		}

	public: // Methods
		FD3D12OfflineDescriptorManager(FRHIGPUMask Node, D3D12_DESCRIPTOR_HEAP_TYPE Type, uint32_t NumDescriptorsPerHeap)
			: m_Desc(CreateDescriptor(Node, Type, NumDescriptorsPerHeap))
			, m_DescriptorSize(0)
			, m_pDevice(nullptr)
		{}

		void Init(ID3D12Device* pDevice)
		{
			m_pDevice = pDevice;
			m_DescriptorSize = pDevice->GetDescriptorHandleIncrementSize(m_Desc.Type);
		}

		HeapOffset AllocateHeapSlot(HeapIndex& outIndex)
		{
			std::lock_guard<std::recursive_mutex> Lock(CritSect);
			if (0 == m_FreeHeaps.size())
			{
				AllocateHeap();
			}
			assert(0 != m_FreeHeaps.size());
			auto Head = m_FreeHeaps.begin();
			outIndex = *Head;
			SHeapEntry& HeapEntry = m_Heaps[outIndex];
			assert(0 != HeapEntry.m_FreeList.size());
			SFreeRange& Range = HeapEntry.m_FreeList.front();
			HeapOffset Ret = { Range.Start };
			Range.Start += m_DescriptorSize;

			if (Range.Start == Range.End)
			{
				HeapEntry.m_FreeList.erase(HeapEntry.m_FreeList.begin());
				if (0 == HeapEntry.m_FreeList.size())
				{
					m_FreeHeaps.erase(Head);
				}
			}
			return Ret;
		}

		void FreeHeapSlot(HeapOffset Offset, HeapIndex index)
		{
			std::lock_guard<std::recursive_mutex> Lock(CritSect);
			SHeapEntry& HeapEntry = m_Heaps[index];

			SFreeRange NewRange =
			{
				Offset.ptr,
				Offset.ptr + m_DescriptorSize
			};

			bool bFound = false;
			for (auto Node = HeapEntry.m_FreeList.begin();
				Node != HeapEntry.m_FreeList.end() && !bFound;
				++Node)
			{
				SFreeRange& Range = *Node;
				assert(Range.Start < Range.End);
				if (Range.Start == Offset.ptr + m_DescriptorSize)
				{
					Range.Start = Offset.ptr;
					bFound = true;
				}
				else if (Range.End == Offset.ptr)
				{
					Range.End += m_DescriptorSize;
					bFound = true;
				}
				else
				{
					assert(Range.End < Offset.ptr || Range.Start > Offset.ptr);
					if (Range.Start > Offset.ptr)
					{
						HeapEntry.m_FreeList.insert(Node, NewRange);
						bFound = true;
					}
				}
			}

			if (!bFound)
			{
				if (0 == HeapEntry.m_FreeList.size())
				{
					m_FreeHeaps.push_back(index);
				}
				HeapEntry.m_FreeList.push_back(NewRange);
			}
		}

	private: // Methods
		void AllocateHeap()
		{
			win32::com_ptr<ID3D12DescriptorHeap> Heap;
			VERIFYD3DRESULT(m_pDevice->CreateDescriptorHeap(&m_Desc, IID_PPV_ARGS(Heap.get_init_ref())));
			//SetName(Heap, L"FD3D12OfflineDescriptorManager Descriptor Heap");

			HeapOffset HeapBase = Heap->GetCPUDescriptorHandleForHeapStart();
			assert(HeapBase.ptr != 0);

			// Allocate and initialize a single new entry in the map
			m_Heaps.resize(m_Heaps.size() + 1);
			SHeapEntry& HeapEntry = m_Heaps.back();
			HeapEntry.m_FreeList.push_back({ HeapBase.ptr,
				HeapBase.ptr + m_Desc.NumDescriptors * m_DescriptorSize });
			HeapEntry.m_Heap = Heap;
			m_FreeHeaps.push_back(m_Heaps.size() - 1);
		}

	private: // Members
		const D3D12_DESCRIPTOR_HEAP_DESC m_Desc;
		uint32_t m_DescriptorSize;
		ID3D12Device* m_pDevice = nullptr; // weak-ref

		THeapMap m_Heaps;
		std::list<HeapIndex> m_FreeHeaps;
		std::recursive_mutex CritSect;
	};

	class FD3D12OnlineHeap : public FD3D12DeviceChild
	{
	public:
		FD3D12OnlineHeap(std::weak_ptr<FD3D12Device> Device, FRHIGPUMask Node, bool CanLoopAround, FD3D12DescriptorCache* _Parent = nullptr);
		virtual ~FD3D12OnlineHeap() { }

		FORCEINLINE D3D12_CPU_DESCRIPTOR_HANDLE GetCPUSlotHandle(uint32_t Slot) const { return{ CPUBase.ptr + Slot * DescriptorSize }; }
		FORCEINLINE D3D12_GPU_DESCRIPTOR_HANDLE GetGPUSlotHandle(uint32_t Slot) const { return{ GPUBase.ptr + Slot * DescriptorSize }; }

		inline const uint32_t GetDescriptorSize() const { return DescriptorSize; }

		const D3D12_DESCRIPTOR_HEAP_DESC& GetDesc() const { return Desc; }

		// Call this to reserve descriptor heap slots for use by the command list you are currently recording. This will wait if
		// necessary until slots are free (if they are currently in use by another command list.) If the reservation can be
		// fulfilled, the index of the first reserved slot is returned (all reserved slots are consecutive.) If not, it will 
		// throw an exception.
		bool CanReserveSlots(uint32_t NumSlots);

		uint32_t ReserveSlots(uint32_t NumSlotsRequested);

		void SetNextSlot(uint32_t NextSlot);

		ID3D12DescriptorHeap* GetHeap() { return Heap.get(); }

		void SetParent(FD3D12DescriptorCache* InParent) { Parent = InParent; }

		// Roll over behavior depends on the heap type
		virtual bool RollOver() = 0;
		virtual void NotifyCurrentCommandList(const D3D12CommandListHandle& CommandListHandle);

		virtual uint32_t GetTotalSize()
		{
			return Desc.NumDescriptors;
		}

		static const uint32_t HeapExhaustedValue = uint32_t(-1);

	protected:

		FD3D12DescriptorCache* Parent;

		D3D12CommandListHandle CurrentCommandList;

		// Handles for manipulation of the heap
		uint32_t DescriptorSize;
		D3D12_CPU_DESCRIPTOR_HANDLE CPUBase;
		D3D12_GPU_DESCRIPTOR_HANDLE GPUBase;

		// This index indicate where the next set of descriptors should be placed *if* there's room
		uint32_t NextSlotIndex;

		// Indicates the last free slot marked by the command list being finished
		uint32_t FirstUsedSlot;

		// Keeping this ptr around is basically just for lifetime management
		win32::com_ptr<ID3D12DescriptorHeap> Heap;

		// Desc contains the number of slots and allows for easy recreation
		D3D12_DESCRIPTOR_HEAP_DESC Desc;

		const bool bCanLoopAround;
	};
}