#pragma once
#include "RHIPrivate/D3D12RHIPrivate.h"
#include "RHI/RDGResourceAccess.h"
#include <cstddef>

namespace RenderCore
{
	class D3D12CommandContext;
	class D3D12Texture2D;
	class D3D12TextureCube;
	struct FD3D12Resource;

	D3D12_RESOURCE_STATES D3D12RdgAccessToResourceState(
		FRDGResourceAccess Access,
		const D3D12Texture2D* Tex2D,
		bool bAsyncCompute,
		bool bStructuredBufferNonPixelSrv = false);

	void D3D12RdgApplyTextureBarrier(D3D12CommandContext& Ctx, const FRDGTextureBarrierDesc& Desc, ERDGPassQueue PassQueue);
	void D3D12RdgApplyTextureBarriers(D3D12CommandContext& Ctx, const FRDGTextureBarrierDesc* Items, size_t Count, ERDGPassQueue PassQueue);
}
