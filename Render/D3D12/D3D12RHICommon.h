#pragma once
#include "RHI/RHI.h"

namespace RenderCore
{
	class D3D12Adapter;

	// Defines a unique command queue type within a FD3D12Device (owner by the command list managers).
	enum class ED3D12CommandQueueType
	{
		Default,
		Copy,
		Async
	};

	class D3D12AdapterChild
	{
	protected:
		std::weak_ptr<D3D12Adapter> ParentAdapter;

	public:
		D3D12AdapterChild(std::weak_ptr<D3D12Adapter> InParent ) : ParentAdapter(InParent) {}

		FORCEINLINE std::shared_ptr<D3D12Adapter> GetParentAdapter() const
		{
			// If this fires an object was likely created with a default constructor i.e in an STL container
			// and is therefore an orphan
			assert(!ParentAdapter.expired());
			return ParentAdapter.lock();
		}

		// To be used with delayed setup
		inline void SetParentAdapter(std::weak_ptr<D3D12Adapter> InParent)
		{
			assert(ParentAdapter.expired());
			ParentAdapter = InParent;
		}
	};

}