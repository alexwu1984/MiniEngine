#include "D3D12/D3D12RDGBarriers.h"
#include "D3D12/D3D12CommandContext.h"
#include "D3D12/D3D12Texture2D.h"
#include "D3D12/D3D12TextureCube.h"
#include "D3D12/D3D12Resource.h"
#include "D3D12/D3D12ReourceTraits.h"
#include "RHI/RHI.h"
#include "RHI/RHIDefinitions.h"

namespace RenderCore
{
	namespace
	{
		bool IsDepthStencilPixelFormat(EPixelFormat PF)
		{
			switch (PF)
			{
			case PF_DepthStencil:
			case PF_ShadowDepth:
			case PF_D24:
				return true;
			default:
				return false;
			}
		}

		D3D12_RESOURCE_STATES ShaderReadableStateForTexture2DSample(const D3D12Texture2D* Tex2D, bool bAsyncCompute)
		{
			if (!Tex2D)
				return D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
			return bAsyncCompute ? D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE : D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
		}

		bool ShouldUsePlanarDepthSrvBarrierLoop(const D3D12Texture2D* Tex2D, FD3D12Resource* Res)
		{
			return Tex2D && Res && IsDepthStencilPixelFormat(Tex2D->GetPixelFormat()) && Res->GetPlaneCount() > 1;
		}

		static UINT CalcSubresourceDx12(UINT MipSlice, UINT ArraySlice, UINT PlaneSlice, UINT MipLevels, UINT ArraySize)
		{
			return MipSlice + ArraySlice * MipLevels + PlaneSlice * MipLevels * ArraySize;
		}
	}

	D3D12_RESOURCE_STATES D3D12RdgAccessToResourceState(
		FRDGResourceAccess Access,
		const D3D12Texture2D* Tex2D,
		bool bAsyncCompute,
		bool bStructuredBufferNonPixelSrv)
	{
		switch (Access)
		{
		case FRDGResourceAccess::SRV:
			return ShaderReadableStateForTexture2DSample(Tex2D, bAsyncCompute || bStructuredBufferNonPixelSrv);
		case FRDGResourceAccess::UAV:
			return D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
		case FRDGResourceAccess::RTV:
			return D3D12_RESOURCE_STATE_RENDER_TARGET;
		case FRDGResourceAccess::DSV:
			return D3D12_RESOURCE_STATE_DEPTH_WRITE;
		case FRDGResourceAccess::CopySrc:
			return D3D12_RESOURCE_STATE_COPY_SOURCE;
		case FRDGResourceAccess::CopyDst:
			return D3D12_RESOURCE_STATE_COPY_DEST;
		default:
			return D3D12_RESOURCE_STATE_COMMON;
		}
	}

	void D3D12RdgApplyTextureBarrier(D3D12CommandContext& Ctx, const FRDGTextureBarrierDesc& D, ERDGPassQueue PassQueue)
	{
		if (D.Access == FRDGResourceAccess::Unknown)
			return;

		const bool bAsyncCompute = (PassQueue == ERDGPassQueue::AsyncCompute);
		const bool bHasCube = static_cast<bool>(D.TextureCube);
		const bool bHas2D = static_cast<bool>(D.Texture);
		if (!bHasCube && !bHas2D)
			return;

		D3D12Texture2D* Tex2D = bHas2D ? RHIResourceCast(D.Texture.get()) : nullptr;
		D3D12TextureCube* TexCube = bHasCube ? RHIResourceCast(D.TextureCube.get()) : nullptr;
		FD3D12Resource* Res = Tex2D ? Tex2D->GetResource() : (TexCube ? TexCube->GetResource() : nullptr);
		if (!Res || !Res->RequiresResourceStateTracking())
			return;

		D3D12_RESOURCE_STATES Target = D3D12_RESOURCE_STATE_COMMON;
		bool bSrvPlanarDepthBarrierLoop = false;
		switch (D.Access)
		{
		case FRDGResourceAccess::SRV:
			if (Tex2D)
			{
				Target = ShaderReadableStateForTexture2DSample(Tex2D, bAsyncCompute);
				bSrvPlanarDepthBarrierLoop = ShouldUsePlanarDepthSrvBarrierLoop(Tex2D, Res);
			}
			else
				Target = bAsyncCompute ? D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE : D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
			break;
		case FRDGResourceAccess::UAV:
			Target = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
			break;
		case FRDGResourceAccess::RTV:
			Target = D3D12_RESOURCE_STATE_RENDER_TARGET;
			break;
		case FRDGResourceAccess::DSV:
			Target = D3D12_RESOURCE_STATE_DEPTH_WRITE;
			break;
		case FRDGResourceAccess::CopySrc:
			Target = D3D12_RESOURCE_STATE_COPY_SOURCE;
			break;
		case FRDGResourceAccess::CopyDst:
			Target = D3D12_RESOURCE_STATE_COPY_DEST;
			break;
		default:
			return;
		}

		if (D.SubresourceIndex == FRDGAllSubresources)
		{
			if (Tex2D && bSrvPlanarDepthBarrierLoop && D.Access == FRDGResourceAccess::SRV)
			{
				const UINT mipLevels = Res->GetMipLevels();
				const UINT arraySize = Res->GetArraySize();
				const UINT planeCount = Res->GetPlaneCount();
				for (UINT plane = 0u; plane < planeCount; ++plane)
					for (UINT arr = 0u; arr < arraySize; ++arr)
						Ctx.TransitionSubResource(Res, Target, CalcSubresourceDx12(0u, arr, plane, mipLevels, arraySize), false);
			}
			else
				Ctx.TransitionResource(Res, Target, false);
		}
		else
			Ctx.TransitionSubResource(Res, Target, D.SubresourceIndex, false);
	}

	void D3D12RdgApplyTextureBarriers(D3D12CommandContext& Ctx, const FRDGTextureBarrierDesc* Items, size_t Count, ERDGPassQueue PassQueue)
	{
		if (!Items || Count == 0)
			return;
		for (size_t i = 0; i < Count; ++i)
			D3D12RdgApplyTextureBarrier(Ctx, Items[i], PassQueue);
	}
}
