#include "RHIPrivate/D3D12RHIPrivate.h"
#include "D3D12/D3D12DirectCommandListManager.h"

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

}
