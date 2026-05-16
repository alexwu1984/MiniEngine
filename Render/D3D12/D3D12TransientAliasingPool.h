#pragma once
#include "RHIPrivate/D3D12RHIPrivate.h"

struct ID3D12Heap;

namespace RenderCore
{
	class FD3D12Adapter;
	class FD3D12Resource;
	class FD3D12TransientAliasingPool;

	struct FD3D12AliasingTexLayoutKey
	{
		uint32_t PixelFormat = 0;
		uint32_t DxgiFormat = 0;
		uint32_t Width = 0;
		uint32_t Height = 0;
		uint32_t NumMips = 1;
		uint32_t ResourceFlags = 0;

		bool operator<(const FD3D12AliasingTexLayoutKey& B) const;
	};

	struct FD3D12AliasingSlotLease
	{
		std::shared_ptr<FD3D12TransientAliasingPool> Pool;
		FD3D12AliasingTexLayoutKey Key{};
		size_t ChunkIndex = 0;
		uint32_t SlotIndex = 0;

		~FD3D12AliasingSlotLease();
	};

	/**
	 * Backs frame-scoped RDG UAVs with placed 2D textures in few large DEFAULT heaps so disjoint lifetimes
	 * reuse the same VRAM offsets (texture subresource aliasing at the heap level).
	 */
	class FD3D12TransientAliasingPool final : public std::enable_shared_from_this<FD3D12TransientAliasingPool>
	{
	public:
		explicit FD3D12TransientAliasingPool(std::shared_ptr<FD3D12Adapter> InAdapter);

		/**
		 * Allocates a placed UAV-capable Tex2D matching the usual RHICreateUnorderedAccessView(Format, SizeX, SizeY) layout
		 * (TexCreate_ShaderResource | TexCreate_UAV, full mips = 1 unless you change the pool).
		 */
		HRESULT TryAllocatePlacedUAVTexture2D(
			EPixelFormat InFormat,
			int32_t SizeX,
			int32_t SizeY,
			FD3D12Resource** OutResource,
			std::shared_ptr<FD3D12AliasingSlotLease>* OutLease,
			const wchar_t* DebugName);

		/** Drop heaps whose slots are all free and GPU-retired; call after frame fence advances. */
		void TrimEmptyChunks();

	private:
		friend struct FD3D12AliasingSlotLease;
		void ReleaseSlot(const FD3D12AliasingTexLayoutKey& Key, size_t ChunkIndex, uint32_t SlotIndex);

		struct FChunk
		{
			ID3D12Heap* Heap = nullptr;
			uint64_t HeapSizeBytes = 0;
			uint64_t SlotPitchBytes = 0;
			std::vector<bool> SlotFree;
			std::vector<uint64_t> SlotRetireFence;

			FChunk() = default;
			FChunk(const FChunk&) = delete;
			FChunk& operator=(const FChunk&) = delete;
			FChunk(FChunk&& Other) noexcept
				: Heap(Other.Heap)
				, HeapSizeBytes(Other.HeapSizeBytes)
				, SlotPitchBytes(Other.SlotPitchBytes)
				, SlotFree(std::move(Other.SlotFree))
				, SlotRetireFence(std::move(Other.SlotRetireFence))
			{
				Other.Heap = nullptr;
			}
			FChunk& operator=(FChunk&& Other) noexcept
			{
				if (this != &Other)
				{
					if (Heap)
						Heap->Release();
					Heap = Other.Heap;
					HeapSizeBytes = Other.HeapSizeBytes;
					SlotPitchBytes = Other.SlotPitchBytes;
					SlotFree = std::move(Other.SlotFree);
					SlotRetireFence = std::move(Other.SlotRetireFence);
					Other.Heap = nullptr;
				}
				return *this;
			}
			~FChunk();
		};

		static uint64_t GetSlotRetireFenceValue(const std::shared_ptr<FD3D12Adapter>& Adapter);
		static bool TryPromoteRetiredSlot(FChunk& Ch, uint32_t SlotIndex, const std::shared_ptr<FD3D12Adapter>& Adapter);

		static constexpr uint32_t kSlotsPerChunk = 16;
		static constexpr uint32_t kMaxChunksPerLayout = 2;
		static bool IsChunkFullyFree(FChunk& Ch, const std::shared_ptr<FD3D12Adapter>& Adapter);

		std::weak_ptr<FD3D12Adapter> AdapterWeak;
		std::mutex Mutex;
		std::map<FD3D12AliasingTexLayoutKey, std::vector<FChunk>> LayoutToChunks;
	};
}
