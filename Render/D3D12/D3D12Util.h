#pragma once
/**
 * D3D12 RHI utilities: sync point, resource state, barriers, thread-safe queue,
 * heap helpers, shader quantize types, resource desc helpers, queue mapping.
 */
#include "D3D12/D3D12Limits.h"
#include "D3D12/D3D12RHICommon.h"
#include "RHI/RHIDefinitions.h"
#include <d3d12.h>
#include <intrin.h>
#include <cassert>
#include <cstring>
#include <cstdint>
#include <deque>
#include <mutex>
#include <vector>

namespace RenderCore
{
	class FD3D12Fence;

	class FD3D12SyncPoint
	{
	public:
		explicit FD3D12SyncPoint();
		explicit FD3D12SyncPoint(FD3D12Fence* InFence, uint64_t InValue);

		bool IsValid() const;
		bool IsComplete() const;
		void WaitForCompletion() const;

	private:
		FD3D12Fence* Fence;
		uint64_t Value;
	};

	typedef uint16_t CBVSlotMask;
	static_assert(MAX_ROOT_CBVS <= MAX_CBS, "MAX_ROOT_CBVS must be <= MAX_CBS.");
	static_assert((8 * sizeof(CBVSlotMask)) >= MAX_CBS, "CBVSlotMask isn't large enough to cover all CBs. Please increase the size.");
	static_assert((8 * sizeof(CBVSlotMask)) >= MAX_ROOT_CBVS, "CBVSlotMask isn't large enough to cover all CBs. Please increase the size.");
	static const CBVSlotMask GRootCBVSlotMask = (1 << MAX_ROOT_CBVS) - 1;
	static const CBVSlotMask GDescriptorTableCBVSlotMask = static_cast<CBVSlotMask>(-1) & ~(GRootCBVSlotMask);

#if MAX_SRVS > 32
	typedef uint64_t SRVSlotMask;
#else
	typedef uint32_t SRVSlotMask;
#endif
	static_assert((8 * sizeof(SRVSlotMask)) >= MAX_SRVS, "SRVSlotMask isn't large enough to cover all SRVs. Please increase the size.");

	typedef uint16_t SamplerSlotMask;
	static_assert((8 * sizeof(SamplerSlotMask)) >= MAX_SAMPLERS, "SamplerSlotMask isn't large enough to cover all Samplers. Please increase the size.");

	typedef uint16_t UAVSlotMask;
	static_assert((8 * sizeof(UAVSlotMask)) >= MAX_UAVS, "UAVSlotMask isn't large enough to cover all UAVs. Please increase the size.");

	enum EShaderVisibility
	{
		SV_Vertex,
		SV_Pixel,
		SV_Hull,
		SV_Domain,
		SV_Geometry,
		SV_All,
		SV_ShaderVisibilityCount
	};

	enum ERTRootSignatureType
	{
		RS_Raster,
		RS_RayTracingGlobal,
		RS_RayTracingLocal,
	};

	struct FShaderRegisterCounts
	{
		uint8_t SamplerCount;
		uint8_t ConstantBufferCount;
		uint8_t ShaderResourceCount;
		uint8_t UnorderedAccessCount;
	};

	struct FShaderCodePackedResourceCounts
	{
		static const uint8_t Key = 'p';

		bool bGlobalUniformBufferUsed;
		uint8_t NumSamplers;
		uint8_t NumSRVs;
		uint8_t NumCBs;
		uint8_t NumUAVs;
	};

	template <class Type>
	struct ThreadsafeQueue
	{
	private:
		mutable std::recursive_mutex SynchronizationObject;
		std::deque<Type> Items;
		uint32_t Size = 0;

	public:
		inline const uint32_t GetSize() const { return Size; }

		void Enqueue(const Type& Item)
		{
			std::lock_guard<std::recursive_mutex> ScopeLock(SynchronizationObject);
			Items.push_back(Item);
			Size++;
		}

		bool Dequeue(Type& Result)
		{
			std::lock_guard<std::recursive_mutex> ScopeLock(SynchronizationObject);
			if (Items.empty())
				return false;
			Size--;
			Result = Items.front();
			Items.pop_front();
			return true;
		}

		template <typename CompareFunc>
		bool Dequeue(Type& Result, CompareFunc& Func)
		{
			std::lock_guard<std::recursive_mutex> ScopeLock(SynchronizationObject);
			if (Items.empty())
				return false;
			Result = Items.back();
			if (Func(Result))
			{
				Size--;
				Result = Items.front();
				Items.pop_front();
				return true;
			}
			return false;
		}

		template <typename CompareFunc>
		bool BatchDequeue(std::deque<Type>* Result, CompareFunc& Func, uint32_t MaxItems)
		{
			std::lock_guard<std::recursive_mutex> ScopeLock(SynchronizationObject);
			uint32_t i = 0;
			Type Item;
			while (!Items.empty() && i <= MaxItems)
			{
				Item = Items.back();
				if (Func(Item))
				{
					Size--;
					Result = Items.front();
					Items.pop_front();
					Result->push_back(Item);
					i++;
				}
				else
					break;
			}
			return i > 0;
		}

		bool Peek(Type& Result)
		{
			std::lock_guard<std::recursive_mutex> ScopeLock(SynchronizationObject);
			if (Items.empty())
				return false;
			return Items.back();
		}

		bool IsEmpty()
		{
			std::lock_guard<std::recursive_mutex> ScopeLock(SynchronizationObject);
			return Items.empty();
		}

		void Empty()
		{
			std::lock_guard<std::recursive_mutex> ScopeLock(SynchronizationObject);
			Items.clear();
		}
	};

	class FD3D12ResourceBarrierBatcher
	{
	public:
		explicit FD3D12ResourceBarrierBatcher() { Barriers.reserve(512); }

		void AddUAV()
		{
			Barriers.push_back({});
			D3D12_RESOURCE_BARRIER& Barrier = Barriers.back();
			Barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
			Barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
			Barrier.UAV.pResource = nullptr;
		}

		void AddTransition(ID3D12Resource* pResource, D3D12_RESOURCE_STATES Before, D3D12_RESOURCE_STATES After, uint32_t Subresource)
		{
			Assert(Before != After);
			Barriers.push_back({});
			D3D12_RESOURCE_BARRIER& Barrier = Barriers.back();
			Barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
			Barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
			Barrier.Transition.StateBefore = Before;
			Barrier.Transition.StateAfter = After;
			Barrier.Transition.Subresource = Subresource;
			Barrier.Transition.pResource = pResource;
		}

		void AddAliasingBarrier(ID3D12Resource* pResource)
		{
			Barriers.push_back({});
			D3D12_RESOURCE_BARRIER& Barrier = Barriers.back();
			Barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_ALIASING;
			Barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
			Barrier.Aliasing.pResourceBefore = NULL;
			Barrier.Aliasing.pResourceAfter = pResource;
		}

		void Flush(ID3D12GraphicsCommandList* pCommandList)
		{
			if (Barriers.empty())
				return;
			Assert(pCommandList);
			static const UINT kBarrierChunk = 65536;
			const UINT total = (UINT)Barriers.size();
			const D3D12_RESOURCE_BARRIER* ptr = Barriers.data();
			UINT remaining = total;
			while (remaining > 0)
			{
				const UINT n = (remaining > kBarrierChunk) ? kBarrierChunk : remaining;
				pCommandList->ResourceBarrier(n, ptr);
				ptr += n;
				remaining -= n;
			}
			Reset();
		}

		void Reset() { Barriers.clear(); }

		const std::vector<D3D12_RESOURCE_BARRIER>& GetBarriers() const { return Barriers; }

	private:
		std::vector<D3D12_RESOURCE_BARRIER> Barriers;
	};

#define D3D12_RESOURCE_STATE_TBD (D3D12_RESOURCE_STATES)-1
#define D3D12_RESOURCE_STATE_CORRUPT (D3D12_RESOURCE_STATES)-2

	inline bool IsValidD3D12ResourceState(D3D12_RESOURCE_STATES InState)
	{
		return (InState != D3D12_RESOURCE_STATE_TBD && InState != D3D12_RESOURCE_STATE_CORRUPT);
	}

	class CResourceState
	{
	public:
		void Initialize(uint32_t SubresourceCount);

		bool AreAllSubresourcesSame() const;
		bool CheckResourceState(D3D12_RESOURCE_STATES State) const;
		bool CheckResourceStateInitalized() const;
		D3D12_RESOURCE_STATES GetSubresourceState(uint32_t SubresourceIndex) const;
		void SetResourceState(D3D12_RESOURCE_STATES State);
		void SetSubresourceState(uint32_t SubresourceIndex, D3D12_RESOURCE_STATES State);

	private:
		D3D12_RESOURCE_STATES m_ResourceState : 31;
		uint32_t m_AllSubresourcesSame : 1;
		std::vector<D3D12_RESOURCE_STATES> m_SubresourceState;
	};

	struct FD3D12QuantizedBoundShaderState
	{
		FShaderRegisterCounts RegisterCounts[SV_ShaderVisibilityCount];
		ERTRootSignatureType RootSignatureType = RS_Raster;
		bool bAllowIAInputLayout;

		inline bool operator==(const FD3D12QuantizedBoundShaderState& RHS) const
		{
			return 0 == memcmp(this, &RHS, sizeof(RHS));
		}

		bool operator()(const FD3D12QuantizedBoundShaderState& _Left, const FD3D12QuantizedBoundShaderState& _Right) const
		{
			return (GetTypeHash(_Left) < GetTypeHash(_Right));
		}

		static uint32_t GetTypeHash(const FD3D12QuantizedBoundShaderState& Key);

		static void InitShaderRegisterCounts(const D3D12_RESOURCE_BINDING_TIER& ResourceBindingTier, const FShaderCodePackedResourceCounts& Counts, FShaderRegisterCounts& Shader, bool bAllowUAVs = false);
	};

	inline bool IsCPUWritable(D3D12_HEAP_TYPE HeapType, const D3D12_HEAP_PROPERTIES* pCustomHeapProperties = nullptr)
	{
		assert(HeapType == D3D12_HEAP_TYPE_CUSTOM ? pCustomHeapProperties != nullptr : true);
		return HeapType == D3D12_HEAP_TYPE_UPLOAD ||
			(HeapType == D3D12_HEAP_TYPE_CUSTOM &&
				(pCustomHeapProperties->CPUPageProperty == D3D12_CPU_PAGE_PROPERTY_WRITE_COMBINE || pCustomHeapProperties->CPUPageProperty == D3D12_CPU_PAGE_PROPERTY_WRITE_BACK));
	}

	inline bool IsCPUInaccessible(D3D12_HEAP_TYPE HeapType, const D3D12_HEAP_PROPERTIES* pCustomHeapProperties = nullptr)
	{
		assert(HeapType == D3D12_HEAP_TYPE_CUSTOM ? pCustomHeapProperties != nullptr : true);
		return HeapType == D3D12_HEAP_TYPE_DEFAULT ||
			(HeapType == D3D12_HEAP_TYPE_CUSTOM &&
				(pCustomHeapProperties->CPUPageProperty == D3D12_CPU_PAGE_PROPERTY_NOT_AVAILABLE));
	}

	inline D3D12_RESOURCE_STATES DetermineInitialResourceState(D3D12_HEAP_TYPE HeapType, const D3D12_HEAP_PROPERTIES* pCustomHeapProperties = nullptr)
	{
		if (HeapType == D3D12_HEAP_TYPE_UPLOAD)
			return D3D12_RESOURCE_STATE_COMMON;
		if (HeapType == D3D12_HEAP_TYPE_DEFAULT || IsCPUWritable(HeapType, pCustomHeapProperties))
			return D3D12_RESOURCE_STATE_GENERIC_READ;
		assert(HeapType == D3D12_HEAP_TYPE_READBACK);
		return D3D12_RESOURCE_STATE_COPY_DEST;
	}

#include "D3D12/D3D12FormatUtil.h"

	inline D3D12_RESOURCE_FLAGS CombineResourceFlags(int32_t TexFlags)
	{
		D3D12_RESOURCE_FLAGS Flags = D3D12_RESOURCE_FLAG_NONE;
		if (TexFlags & TexCreate_DepthStencilTargetable)
			Flags |= D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
		if (TexFlags & TexCreate_UAV)
			Flags |= D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
		if (TexFlags & TexCreate_RenderTargetable)
			Flags |= D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
		return Flags;
	}

	inline D3D12_RESOURCE_DESC DescribeTex2D(uint32_t Width, uint32_t Height, uint32_t DepthOrArraySize, uint32_t NumMips, DXGI_FORMAT Format, UINT Flags)
	{
		D3D12_RESOURCE_DESC Desc = {};
		Desc.Alignment = 0;
		Desc.DepthOrArraySize = (UINT16)DepthOrArraySize;
		Desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
		Desc.Flags = (D3D12_RESOURCE_FLAGS)Flags;
		Desc.Format = Format;
		Desc.Width = (UINT)Width;
		Desc.Height = (UINT)Height;
		Desc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
		Desc.MipLevels = (UINT16)NumMips;
		Desc.SampleDesc.Count = 1;
		Desc.SampleDesc.Quality = 0;
		return Desc;
	}

	inline uint32_t ComputeNumMips(uint32_t Width, uint32_t Height)
	{
		uint32_t HighBit = 0;
		_BitScanReverse((unsigned long*)&HighBit, Width | Height);
		return HighBit + 1;
	}

#define GET_QUEUE_TYPE(f) ((D3D12_COMMAND_LIST_TYPE)(f >> 56))

	inline ED3D12CommandQueueType GetCommandQueueType(D3D12_COMMAND_LIST_TYPE Type)
	{
		switch (Type)
		{
		case D3D12_COMMAND_LIST_TYPE_COMPUTE: return ED3D12CommandQueueType::Async;
		case D3D12_COMMAND_LIST_TYPE_COPY: return ED3D12CommandQueueType::Copy;
		default: return ED3D12CommandQueueType::Default;
		}
	}

	uint32_t SSE4_CRC32(const void* Data, size_t NumBytes);

}
