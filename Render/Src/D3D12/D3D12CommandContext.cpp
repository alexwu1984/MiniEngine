#include "D3D12/D3D12CommandContext.h"
#include "D3D12/D3D12Adapter.h"

namespace RenderCore
{

	FD3D12CommandContextBase::FD3D12CommandContextBase(std::weak_ptr<FD3D12Adapter> InParent, bool InIsDefaultContext, bool InIsAsyncComputeContext)
		:FD3D12AdapterChild(InParent)
	{

	}

	D3D12CommandContext::D3D12CommandContext(std::weak_ptr<FD3D12Adapter> InParent, bool InIsDefaultContext, bool InIsAsyncComputeContext)
		:FD3D12CommandContextBase(InParent,InIsDefaultContext,InIsAsyncComputeContext)
	{

	}

}
