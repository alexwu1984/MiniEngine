#include "RHIPrivate/D3D12RHIPrivate.h"
#include "D3D12/D3D12DirectCommandListManager.h"
#include "math/math.h"

namespace RenderCore
{

	bool D3D12SyncPoint::IsValid() const
	{
		return Fence != nullptr;
	}

	bool D3D12SyncPoint::IsComplete() const
	{
		assert(IsValid());
		return Fence->IsFenceComplete(Value);
	}

	void D3D12SyncPoint::WaitForCompletion() const
	{
		assert(IsValid());
		Fence->WaitForFence(Value);
	}

	void CResourceState::Initialize(uint32_t SubresourceCount)
	{
		assert(0 == m_SubresourceState.size());

		// Allocate space for per-subresource tracking structures
		assert(SubresourceCount > 0);
		m_SubresourceState.resize(SubresourceCount);
		assert(m_SubresourceState.size() == SubresourceCount);

		// All subresources start out in an unknown state
		SetResourceState(D3D12_RESOURCE_STATE_TBD);
	}

	bool CResourceState::AreAllSubresourcesSame() const
	{
		return m_AllSubresourcesSame && (m_ResourceState != D3D12_RESOURCE_STATE_TBD);
	}

	bool CResourceState::CheckResourceState(D3D12_RESOURCE_STATES State) const
	{
		if (m_AllSubresourcesSame)
		{
			return State == m_ResourceState;
		}
		else
		{
			// All subresources must be individually checked
			const uint32_t numSubresourceStates = m_SubresourceState.size();
			for (uint32_t i = 0; i < numSubresourceStates; i++)
			{
				if (m_SubresourceState[i] != State)
				{
					return false;
				}
			}

			return true;
		}
	}

	bool CResourceState::CheckResourceStateInitalized() const
	{
		return m_SubresourceState.size() > 0;
	}

	D3D12_RESOURCE_STATES CResourceState::GetSubresourceState(uint32_t SubresourceIndex) const
	{
		if (m_AllSubresourcesSame)
		{
			return m_ResourceState;
		}
		else
		{
			assert(SubresourceIndex < static_cast<uint32_t>(m_SubresourceState.size()));
			return m_SubresourceState[SubresourceIndex];
		}
	}

	void CResourceState::SetResourceState(D3D12_RESOURCE_STATES State)
	{
		m_AllSubresourcesSame = 1;
		m_ResourceState = State;
	}

	void CResourceState::SetSubresourceState(uint32_t SubresourceIndex, D3D12_RESOURCE_STATES State)
	{
		// If setting all subresources, or the resource only has a single subresource, set the per-resource state
		if (SubresourceIndex == D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES ||
			m_SubresourceState.size() == 1)
		{
			SetResourceState(State);
		}
		else
		{
			assert(SubresourceIndex < static_cast<uint32_t>(m_SubresourceState.size()));

			// If state was previously tracked on a per-resource level, then transition to per-subresource tracking
			if (m_AllSubresourcesSame)
			{
				const uint32_t numSubresourceStates = m_SubresourceState.size();
				for (uint32_t i = 0; i < numSubresourceStates; i++)
				{
					m_SubresourceState[i] = m_ResourceState;
				}

				m_AllSubresourcesSame = 0;

				// State is now tracked per-subresource, so m_ResourceState should not be read.
			}
			m_SubresourceState[SubresourceIndex] = State;
		}
	}

	uint32_t SSE4_CRC32(const void* Data, size_t NumBytes)
	{
		uint32_t Hash = 0;
#if defined(_WIN64)
		static const size_t Alignment = 8;//64 Bit
#elif defined(_WIN32)
		static const size_t Alignment = 4;//32 Bit
#else
		assert(0);
		return 0;
#endif

		const size_t RoundingIterations = (NumBytes & (Alignment - 1));
		uint8_t* UnalignedData = (uint8_t*)Data;
		for (size_t i = 0; i < RoundingIterations; i++)
		{
			Hash = _mm_crc32_u8(Hash, UnalignedData[i]);
		}
		UnalignedData += RoundingIterations;
		NumBytes -= RoundingIterations;

		size_t* AlignedData = (size_t*)UnalignedData;
		assert((NumBytes % Alignment) == 0);
		const size_t NumIterations = (NumBytes / Alignment);
		for (size_t i = 0; i < NumIterations; i++)
		{
#ifdef _WIN64
			Hash = _mm_crc32_u64(Hash, AlignedData[i]);
#else
			Hash = _mm_crc32_u32(Hash, AlignedData[i]);
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

		// Round up and clamp values to their max
		// Note: Rounding and setting counts based on binding tier allows us to create fewer root signatures.

		// To reduce the size of the root signature, we only allow UAVs for certain shaders. 
		// This code makes the assumption that the engine only uses UAVs at the PS or CS shader stages.
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
