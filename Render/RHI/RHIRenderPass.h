#pragma once

/**
 * Phase 0 - OM / render-target discipline (moving toward UE-style pass boundaries):
 * - Treat RTV/DSV binding changes as pass-level decisions: prefer RHIBeginRenderPass (or one explicit SetRenderTarget pair)
 *   at pass entry instead of scattering binds between draws inside a logical pass.
 * - Descriptor tables / shader bindings remain separate; they still batch per backend rules.
 *
 * Phase 1 - Minimal render-pass shell:
 * FRHIRenderPassDesc + RHIBeginRenderPass centralizes SetRenderTarget + full viewport setup so callers share one path.
 *
 * Phase 2 - Declared texture uses (SRV/RTV/UAV/...) via DeclaredTextureBarriers:
 * Applied before OM binds through RHIRenderPassApplyDeclaredTextureBarriers (D3D12 uses same rules as RDGApplyPassBeginBarriers).
 *
 * Phase 3 - Same barrier list for RDG and immediate passes:
 * FRDGUtils::AppendPassTextureBarriers mirrors RDG pass IO -> FRDGTextureBarrierDesc; fullscreen raster (e.g. SSR) uses
 * FRHIRenderPassScope + DeclaredTextureBarriers like DeferredLighting. OM unbind goes through RHIBeginRenderPass({}).
 *
 * Phase 4-5 - Transient pooled resources aligned with UE-style graph lifetime:
 * FRDGBuilder::RegisterTransientUAV plus frame-scoped Acquire/Release around ExecutePasses (RenderTexturePool) lets passes borrow UAVs for
 * the graph segment rather than hoarding pooled handles only in subsystem objects.
 *
 * Phase 6 - Barrier single-source policy: derive transitions from RDG FRDGPassResource.Access and RHIRenderPass DeclaredTextureBarriers;
 * implicit transitions inside SetRenderTarget remain a backend fallback, not the primary scheduling surface.
 *
 * Phase 7 - Binding discipline removed dead RHIBatched* hooks; Fur forward retains explicit shared-SRV pinning when pixel shader identity changes.
 */

#include "RHI/RHICommandContext.h"
#include "RHI/RHITexture2D.h"
#include <memory>
#include <utility>
#include <vector>

namespace RenderCore
{

	struct FRHIRenderPassDesc
	{
		std::vector<std::shared_ptr<RHITexture2D>> ColorTargets;
		std::shared_ptr<RHITexture2D> DepthStencil;
		bool bBindDepthStencil = true;
		int32_t ViewportOffsetX = 0;
		int32_t ViewportOffsetY = 0;
		/** Non-positive: derive full rect from first non-null color target (depth-only passes must set explicitly). */
		int32_t ViewportWidth = -1;
		int32_t ViewportHeight = -1;
		const char* DebugName = nullptr;
		/** Optional: transitions flushed before SetRenderTarget (whole-resource when SubresourceIndex is 0xFFFFFFFF). */
		std::vector<FRDGTextureBarrierDesc> DeclaredTextureBarriers;

		static FRHIRenderPassDesc SingleColor(std::shared_ptr<RHITexture2D> Color, std::shared_ptr<RHITexture2D> Depth)
		{
			FRHIRenderPassDesc D;
			if (Color)
				D.ColorTargets.push_back(std::move(Color));
			D.DepthStencil = std::move(Depth);
			D.bBindDepthStencil = static_cast<bool>(D.DepthStencil);
			return D;
		}

		static FRHIRenderPassDesc SingleColorNoDepth(std::shared_ptr<RHITexture2D> Color)
		{
			return SingleColor(std::move(Color), nullptr);
		}

		/** MRT (+ optional depth): binds all entries in Colors; Depth null means omit DSV. */
		static FRHIRenderPassDesc ColorTargetsAndDepth(const std::vector<std::shared_ptr<RHITexture2D>>& Colors, std::shared_ptr<RHITexture2D> Depth)
		{
			FRHIRenderPassDesc D;
			D.ColorTargets = Colors;
			D.DepthStencil = std::move(Depth);
			D.bBindDepthStencil = static_cast<bool>(D.DepthStencil);
			return D;
		}
	};

	inline void RHIBeginRenderPass(RHICommandContext& Ctx, const FRHIRenderPassDesc& Desc)
	{
		if (Desc.DebugName)
			Ctx.BeginUserMark(Desc.DebugName);

		if (!Desc.DeclaredTextureBarriers.empty())
			Ctx.RHIRenderPassApplyDeclaredTextureBarriers(Desc.DeclaredTextureBarriers.data(), Desc.DeclaredTextureBarriers.size(), ERDGPassQueue::Graphics);

		std::shared_ptr<RHITexture2D> DepthBind = (Desc.bBindDepthStencil ? Desc.DepthStencil : nullptr);
		if (!Desc.ColorTargets.empty())
			Ctx.SetRenderTarget(Desc.ColorTargets, DepthBind);
		else if (DepthBind)
			Ctx.SetRenderTarget(std::vector<std::shared_ptr<RHITexture2D>>{}, DepthBind);
		else
			Ctx.SetRenderTarget(std::vector<std::shared_ptr<RHITexture2D>>{}, nullptr);

		int32_t W = Desc.ViewportWidth;
		int32_t H = Desc.ViewportHeight;
		if (W <= 0 || H <= 0)
		{
			for (const auto& T : Desc.ColorTargets)
			{
				if (T)
				{
					const core::vec2i Sz = T->GetSize();
					W = Sz.x;
					H = Sz.y;
					break;
				}
			}
		}
		if (W > 0 && H > 0)
			Ctx.SetViewPort(Desc.ViewportOffsetX, Desc.ViewportOffsetY, W, H);
	}

	inline void RHIEndRenderPass(RHICommandContext& Ctx, const FRHIRenderPassDesc& Desc)
	{
		if (Desc.DebugName)
			Ctx.EndUserMark();
	}

	class FRHIRenderPassScope
	{
	public:
		FRHIRenderPassScope(RHICommandContext& InCtx, FRHIRenderPassDesc InDesc)
			: Ctx(InCtx), Desc(std::move(InDesc))
		{
			RHIBeginRenderPass(Ctx, Desc);
		}

		~FRHIRenderPassScope()
		{
			RHIEndRenderPass(Ctx, Desc);
		}

		FRHIRenderPassScope(const FRHIRenderPassScope&) = delete;
		FRHIRenderPassScope& operator=(const FRHIRenderPassScope&) = delete;

	private:
		RHICommandContext& Ctx;
		FRHIRenderPassDesc Desc;
	};

} // namespace RenderCore
