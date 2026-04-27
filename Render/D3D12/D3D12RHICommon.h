#pragma once
#include "RHI/RHI.h"
#include <d3d12.h>

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

	// UE4 FD3D12Device: parallel fast-allocator channels (DEFAULT heap vs UPLOAD heap linear pages).
	enum EFastAllocatorType
	{
		InvalidFastAllocator = -1,
		/** DEFAULT heap linear-buffer pool (GPU-resident ring / scratch). */
		DefaultFastAllocator = 0,
		/** UPLOAD heap pool (UE DefaultFastAllocator upload path). */
		UploadFastAllocator = 1,
		FastAllocator_Num,
	};

	class FD3D12AdapterChild
	{
	protected:
		std::weak_ptr<FD3D12Adapter> ParentAdapter;

	public:
		FD3D12AdapterChild(std::weak_ptr<FD3D12Adapter> InParent ) : ParentAdapter(InParent) {}

		// Safe variant for shutdown paths where the adapter may already be destroyed.
		FORCEINLINE std::shared_ptr<FD3D12Adapter> TryGetParentAdapter() const
		{
			return ParentAdapter.lock();
		}

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