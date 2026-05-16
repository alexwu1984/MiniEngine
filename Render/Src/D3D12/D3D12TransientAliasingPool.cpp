#include "D3D12/D3D12TransientAliasingPool.h"
#include "D3D12/D3D12Adapter.h"
#include "D3D12/D3D12FormatUtil.h"
#include "D3D12/D3D12Resource.h"
#include "D3D12/D3D12Util.h"
#include "core/strings.h"

namespace RenderCore
{
	namespace
	{
		static UINT64 AlignUINT64(UINT64 V, UINT64 A)
		{
			if (A <= 1)
				return V;
			return (V + A - 1uLL) / A * A;
		}

		static void SafeReleaseHeap(ID3D12Heap*& H)
		{
			if (H)
			{
				H->Release();
				H = nullptr;
			}
		}
	}

	bool FD3D12AliasingTexLayoutKey::operator<(const FD3D12AliasingTexLayoutKey& B) const
	{
		return std::tie(DxgiFormat, Width, Height, NumMips, ResourceFlags)
			< std::tie(B.DxgiFormat, B.Width, B.Height, B.NumMips, B.ResourceFlags);
	}

	FD3D12AliasingSlotLease::~FD3D12AliasingSlotLease()
	{
		if (Pool)
			Pool->ReleaseSlot(Key, ChunkIndex, SlotIndex);
	}

	FD3D12TransientAliasingPool::FChunk::~FChunk()
	{
		SafeReleaseHeap(Heap);
	}

	FD3D12TransientAliasingPool::FD3D12TransientAliasingPool(std::shared_ptr<FD3D12Adapter> InAdapter)
		: AdapterWeak(InAdapter)
	{
	}

	void FD3D12TransientAliasingPool::ReleaseSlot(const FD3D12AliasingTexLayoutKey& Key, size_t ChunkIndex, uint32_t SlotIndex)
	{
		std::lock_guard<std::mutex> Lock(Mutex);
		auto ItLayout = LayoutToChunks.find(Key);
		if (ItLayout == LayoutToChunks.end())
			return;
		std::vector<FChunk>& Chunks = ItLayout->second;
		if (ChunkIndex >= Chunks.size())
			return;
		FChunk& Ch = Chunks[ChunkIndex];
		if (SlotIndex >= Ch.SlotFree.size())
			return;
		Ch.SlotFree[SlotIndex] = true;
	}

	HRESULT FD3D12TransientAliasingPool::TryAllocatePlacedUAVTexture2D(
		EPixelFormat InFormat,
		int32_t SizeX,
		int32_t SizeY,
		FD3D12Resource** OutResource,
		std::shared_ptr<FD3D12AliasingSlotLease>* OutLease,
		const wchar_t* DebugName)
	{
		if (!OutResource || !OutLease)
			return E_POINTER;
		*OutResource = nullptr;
		*OutLease = nullptr;

		std::shared_ptr<FD3D12Adapter> Adapter = AdapterWeak.lock();
		if (!Adapter || SizeX <= 0 || SizeY <= 0)
			return E_INVALIDARG;

		ID3D12Device* Device = Adapter->GetD3DDevice();
		if (!Device)
			return E_FAIL;

		const int32_t InFlags = (int32_t)(ETextureCreateFlags::TexCreate_UAV | ETextureCreateFlags::TexCreate_ShaderResource);
		DXGI_FORMAT PlatformResourceFormat = FindSharedResourceDXGIFormat((DXGI_FORMAT)GPixelFormats[InFormat].PlatformFormat, false);

		const uint32_t NumMips = 1;
		D3D12_RESOURCE_FLAGS ResFlags = CombineResourceFlags(InFlags);
		D3D12_RESOURCE_DESC ResDesc = DescribeTex2D((uint32_t)SizeX, (uint32_t)SizeY, 1, NumMips, PlatformResourceFormat, ResFlags);
		ResDesc.SampleDesc.Count = 1;
		ResDesc.SampleDesc.Quality = 0;

		D3D12_RESOURCE_ALLOCATION_INFO AllocInfo = Device->GetResourceAllocationInfo(0, 1, &ResDesc);
		if (AllocInfo.SizeInBytes == UINT64_MAX || AllocInfo.SizeInBytes == 0)
			return E_FAIL;

		UINT64 SlotPitchBytes = AlignUINT64(AllocInfo.SizeInBytes, AllocInfo.Alignment);
		SlotPitchBytes = AlignUINT64(SlotPitchBytes, (UINT64)D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT);

		const UINT64 ChunkHeapBytesUnaligned = SlotPitchBytes * (UINT64)kSlotsPerChunk;
		const UINT64 ChunkHeapBytes = AlignUINT64(ChunkHeapBytesUnaligned, (UINT64)D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT);

		FD3D12AliasingTexLayoutKey Key{};
		Key.DxgiFormat = (uint32_t)PlatformResourceFormat;
		Key.Width = (uint32_t)SizeX;
		Key.Height = (uint32_t)SizeY;
		Key.NumMips = NumMips;
		Key.ResourceFlags = (uint32_t)ResFlags;

		std::lock_guard<std::mutex> Lock(Mutex);

		auto TryPlaceInChunks = [&](std::vector<FChunk>& Chunks, FD3D12Resource** OutRes,
									std::shared_ptr<FD3D12AliasingSlotLease>* OutLe) -> HRESULT {
			for (size_t ci = 0; ci < Chunks.size(); ++ci)
			{
				FChunk& Ch = Chunks[ci];
				for (uint32_t si = 0; si < Ch.SlotFree.size(); ++si)
				{
					if (!Ch.SlotFree[si])
						continue;

					const UINT64 HeapOffset = Ch.SlotPitchBytes * (UINT64)si;
					HRESULT hrPlace = Adapter->CreatePlacedResource(Ch.Heap, HeapOffset, ResDesc,
						D3D12_RESOURCE_STATE_COPY_DEST, nullptr, OutRes, DebugName ? DebugName : L"TransientAliasingUAV");
					if (FAILED(hrPlace))
						continue;

					Ch.SlotFree[si] = false;

					auto Lease = std::make_shared<FD3D12AliasingSlotLease>();
					Lease->Pool = shared_from_this();
					Lease->Key = Key;
					Lease->ChunkIndex = ci;
					Lease->SlotIndex = si;
					*OutLe = std::move(Lease);
					return S_OK;
				}
			}
			return E_FAIL;
		};

		std::vector<FChunk>& Chunks = LayoutToChunks[Key];
		HRESULT hrExisting = TryPlaceInChunks(Chunks, OutResource, OutLease);
		if (SUCCEEDED(hrExisting))
			return S_OK;

		FChunk NewChunk{};
		NewChunk.SlotPitchBytes = SlotPitchBytes;
		NewChunk.SlotFree.assign(kSlotsPerChunk, true);

		D3D12_HEAP_PROPERTIES HeapProps = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
		D3D12_HEAP_DESC HeapDesc{};
		HeapDesc.SizeInBytes = ChunkHeapBytes;
		HeapDesc.Properties = HeapProps;
		HeapDesc.Alignment = 0;

		static std::atomic_uint32_t sHeapCounter = 0;
		const std::wstring HeapName = core::formatw(L"TransientAliasingHeap_", ++sHeapCounter);

		const D3D12_RESOURCE_HEAP_TIER HeapTier = Adapter->GetResourceHeapTier();

		// Heaps that include D3D12_HEAP_FLAG_DENY_NON_RT_DS_TEXTURES only admit RT/DS textures (#638 for UAV).
		// Do not use D3D12_HEAP_FLAG_DENY_BUFFERS alone as a fallback - Tier rules want NONE (Tier 2) or
		// ALLOW_ONLY_NON_RT_DS_TEXTURES (= DENY_BUFFERS | DENY_RT_DS_TEXTURES, no DENY_NON_RT_DS bit).
		auto TryHeapCategory = [&](D3D12_HEAP_FLAGS Flags) -> HRESULT {
			HeapDesc.Flags = Flags;
			SafeReleaseHeap(NewChunk.Heap);
			const HRESULT hr = Adapter->CreateHeap(HeapDesc, &NewChunk.Heap, HeapName.c_str());
			if (FAILED(hr) || !NewChunk.Heap)
				return FAILED(hr) ? hr : E_FAIL;
			const D3D12_HEAP_FLAGS Actual = NewChunk.Heap->GetDesc().Flags;
			if ((Actual & D3D12_HEAP_FLAG_DENY_NON_RT_DS_TEXTURES) != 0)
			{
				SafeReleaseHeap(NewChunk.Heap);
				return E_FAIL;
			}
			return S_OK;
		};

		HRESULT hrHeap = E_FAIL;
		if (HeapTier >= D3D12_RESOURCE_HEAP_TIER_2)
		{
			hrHeap = TryHeapCategory(D3D12_HEAP_FLAG_NONE);
			if (FAILED(hrHeap))
				hrHeap = TryHeapCategory(D3D12_HEAP_FLAG_ALLOW_ONLY_NON_RT_DS_TEXTURES);
		}
		else
		{
			hrHeap = TryHeapCategory(D3D12_HEAP_FLAG_ALLOW_ONLY_NON_RT_DS_TEXTURES);
			if (FAILED(hrHeap))
				hrHeap = TryHeapCategory(D3D12_HEAP_FLAG_NONE);
		}

		if (FAILED(hrHeap) || !NewChunk.Heap)
			return FAILED(hrHeap) ? hrHeap : E_FAIL;

		Chunks.push_back(std::move(NewChunk));
		return TryPlaceInChunks(Chunks, OutResource, OutLease);
	}
}
