#include "D3D12/D3D12Util.h"
#include "D3D12/D3D12DirectCommandListManager.h"
#include "math/math.h"
#include <nmmintrin.h>

namespace RenderCore
{

	FD3D12SyncPoint::FD3D12SyncPoint()
		: Fence(nullptr)
		, Value(0)
	{
	}

	FD3D12SyncPoint::FD3D12SyncPoint(FD3D12Fence* InFence, uint64_t InValue)
		: Fence(InFence)
		, Value(InValue)
	{
	}

	bool FD3D12SyncPoint::IsValid() const
	{
		return Fence != nullptr;
	}

	bool FD3D12SyncPoint::IsComplete() const
	{
		if (!ensureMsgf(IsValid(), "FD3D12SyncPoint::IsComplete: SyncPoint is not valid (Fence is null)."))
			return false;
		return Fence->IsFenceComplete(Value);
	}

	void FD3D12SyncPoint::WaitForCompletion() const
	{
		if (!ensureMsgf(IsValid(), "FD3D12SyncPoint::WaitForCompletion: SyncPoint is not valid (Fence is null)."))
			return;
		Fence->WaitForFence(Value);
	}

	void CResourceState::Initialize(uint32_t SubresourceCount)
	{
		Assert(0 == m_SubresourceState.size());
		Assert(SubresourceCount > 0);
		m_SubresourceState.resize(SubresourceCount);
		Assert(m_SubresourceState.size() == SubresourceCount);
		SetResourceState(D3D12_RESOURCE_STATE_TBD);
	}

	bool CResourceState::AreAllSubresourcesSame() const
	{
		return m_AllSubresourcesSame && (m_ResourceState != D3D12_RESOURCE_STATE_TBD);
	}

	bool CResourceState::CheckResourceState(D3D12_RESOURCE_STATES State) const
	{
		if (m_AllSubresourcesSame)
			return State == m_ResourceState;
		const uint32_t numSubresourceStates = (uint32_t)m_SubresourceState.size();
		for (uint32_t i = 0; i < numSubresourceStates; i++)
		{
			if (m_SubresourceState[i] != State)
				return false;
		}
		return true;
	}

	bool CResourceState::CheckResourceStateInitalized() const
	{
		return m_SubresourceState.size() > 0;
	}

	D3D12_RESOURCE_STATES CResourceState::GetSubresourceState(uint32_t SubresourceIndex) const
	{
		if (SubresourceIndex == D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES)
		{
			if (m_AllSubresourcesSame)
				return m_ResourceState;
			Assert(m_SubresourceState.size() > 0);
			const D3D12_RESOURCE_STATES First = m_SubresourceState[0];
			for (uint32_t i = 1; i < static_cast<uint32_t>(m_SubresourceState.size()); ++i)
				Assert(m_SubresourceState[i] == First);
			return First;
		}
		if (m_AllSubresourcesSame)
			return m_ResourceState;
		assert(SubresourceIndex < static_cast<uint32_t>(m_SubresourceState.size()));
		return m_SubresourceState[SubresourceIndex];
	}

	void CResourceState::SetResourceState(D3D12_RESOURCE_STATES State)
	{
		m_AllSubresourcesSame = 1;
		m_ResourceState = State;
	}

	void CResourceState::SetSubresourceState(uint32_t SubresourceIndex, D3D12_RESOURCE_STATES State)
	{
		if (SubresourceIndex == D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES || m_SubresourceState.size() == 1)
		{
			SetResourceState(State);
		}
		else
		{
			Assert(SubresourceIndex < static_cast<uint32_t>(m_SubresourceState.size()));
			if (m_AllSubresourcesSame)
			{
				const uint32_t numSubresourceStates = (uint32_t)m_SubresourceState.size();
				for (uint32_t i = 0; i < numSubresourceStates; i++)
					m_SubresourceState[i] = m_ResourceState;
				m_AllSubresourcesSame = 0;
			}
			m_SubresourceState[SubresourceIndex] = State;
		}
	}

	uint32_t SSE4_CRC32(const void* Data, size_t NumBytes)
	{
		uint32_t Hash = 0;
#if defined(_WIN64)
		static const size_t Alignment = 8;
#elif defined(_WIN32)
		static const size_t Alignment = 4;
#else
		assert(0);
		return 0;
#endif
		const size_t RoundingIterations = (NumBytes & (Alignment - 1));
		uint8_t* UnalignedData = (uint8_t*)Data;
		for (size_t i = 0; i < RoundingIterations; i++)
			Hash = _mm_crc32_u8(Hash, UnalignedData[i]);
		UnalignedData += RoundingIterations;
		NumBytes -= RoundingIterations;
		size_t* AlignedData = (size_t*)UnalignedData;
		Assert((NumBytes % Alignment) == 0);
		const size_t NumIterations = (NumBytes / Alignment);
		for (size_t i = 0; i < NumIterations; i++)
		{
#ifdef _WIN64
			Hash = _mm_crc32_u64(Hash, AlignedData[i]);
#else
			Hash = _mm_crc32_u32(Hash, (uint32_t)AlignedData[i]);
#endif
		}
		return Hash;
	}

	uint32_t FD3D12QuantizedBoundShaderState::GetTypeHash(const FD3D12QuantizedBoundShaderState& Key)
	{
		return SSE4_CRC32((void*)&Key, sizeof(Key));
	}

	void FD3D12QuantizedBoundShaderState::InitShaderRegisterCounts(const D3D12_RESOURCE_BINDING_TIER& ResourceBindingTier, const FShaderCodePackedResourceCounts& Counts, FShaderRegisterCounts& Shader, bool bAllowUAVs /*= false*/)
	{
		static const uint32_t MaxSamplerCount = MAX_SAMPLERS;
		static const uint32_t MaxConstantBufferCount = MAX_CBS;
		static const uint32_t MaxShaderResourceCount = MAX_SRVS;
		static const uint32_t MaxUnorderedAccessCount = MAX_UAVS;

		assert(bAllowUAVs || (!bAllowUAVs && Counts.NumUAVs == 0));

		if (ResourceBindingTier <= D3D12_RESOURCE_BINDING_TIER_1)
		{
			Shader.SamplerCount = (Counts.NumSamplers > 0) ? math::Min(MaxSamplerCount, math::RoundUpToPowerOfTwo(Counts.NumSamplers)) : Counts.NumSamplers;
			Shader.ShaderResourceCount = (Counts.NumSRVs > 0) ? math::Min(MaxShaderResourceCount, math::RoundUpToPowerOfTwo(Counts.NumSRVs)) : Counts.NumSRVs;
		}
		else
		{
			Shader.SamplerCount = MaxSamplerCount;
			Shader.ShaderResourceCount = MaxShaderResourceCount;
		}

		if (ResourceBindingTier <= D3D12_RESOURCE_BINDING_TIER_2)
		{
			Shader.ConstantBufferCount = (Counts.NumCBs > MAX_ROOT_CBVS) ? math::Min(MaxConstantBufferCount, math::RoundUpToPowerOfTwo(Counts.NumCBs)) : Counts.NumCBs;
			Shader.UnorderedAccessCount = (Counts.NumUAVs > 0 && bAllowUAVs) ? math::Min(MaxUnorderedAccessCount, math::RoundUpToPowerOfTwo(Counts.NumUAVs)) : 0;
		}
		else
		{
			Shader.ConstantBufferCount = (Counts.NumCBs > MAX_ROOT_CBVS) ? MaxConstantBufferCount : Counts.NumCBs;
			Shader.UnorderedAccessCount = (bAllowUAVs) ? MaxUnorderedAccessCount : 0;
		}
	}

}
