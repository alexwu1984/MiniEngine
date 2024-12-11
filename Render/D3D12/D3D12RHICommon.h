#pragma once
#include "RHI/RHI.h"

namespace RenderCore
{
	class FD3D12Adapter;

	// Defines a unique command queue type within a FD3D12Device (owner by the command list managers).
	enum class ED3D12CommandQueueType
	{
		Default,
		Copy,
		Async
	};

	enum ELinearAllocatorType
	{
		InvalidAllocator = -1,
		GpuExclusive = 0,
		CpuWritable = 1,
		NumAllocatorTypes,
	};

	class FD3D12AdapterChild
	{
	protected:
		std::weak_ptr<FD3D12Adapter> ParentAdapter;

	public:
		FD3D12AdapterChild(std::weak_ptr<FD3D12Adapter> InParent ) : ParentAdapter(InParent) {}

		FORCEINLINE std::shared_ptr<FD3D12Adapter> GetParentAdapter() const
		{
			// If this fires an object was likely created with a default constructor i.e in an STL container
			// and is therefore an orphan
			Assert(!ParentAdapter.expired());
			return ParentAdapter.lock();
		}

		// To be used with delayed setup
		inline void SetParentAdapter(std::weak_ptr<FD3D12Adapter> InParent)
		{
			Assert(ParentAdapter.expired());
			ParentAdapter = InParent;
		}
	};

	class FD3D12Device;
	class FD3D12DeviceChild
	{
	protected:
		std::weak_ptr<FD3D12Device> Parent;

	public:
		FD3D12DeviceChild(std::weak_ptr<FD3D12Device> InParent ) : Parent(InParent) {}

		FORCEINLINE std::shared_ptr<FD3D12Device> GetParentDevice() const
		{
			// If this fires an object was likely created with a default constructor i.e in an STL container
			// and is therefore an orphan
			Assert(!Parent.expired());
			return Parent.lock();
		}

		// To be used with delayed setup
		inline void SetParentDevice(std::weak_ptr<FD3D12Device> InParent)
		{
			Assert(Parent.expired());
			Parent = InParent;
		}
	};

}