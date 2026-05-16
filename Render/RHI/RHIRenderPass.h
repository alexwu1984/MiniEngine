#pragma once

/** Render-pass roadmap (RHIRenderPass + RDG): 0 OM discipline, 1 FRHIRenderPass shell, 2 DeclaredTextureBarriers,
 *  3 AppendPassTextureBarriers from FRDGPassDescriptor IO, 4-5 transient pooled UAV + Acquire/Release,
 *  6 barrier metadata single-source, 7 batched-bind hooks removed. */

#include "RHI/RHICommandContext.h"
#include "RHI/RHIRenderTarget.h"
#include "RHI/RHITexture2D.h"

namespace RenderCore
{

	struct FRHIRenderPassDesc
	{
		std::vector<std::shared_ptr<RHITexture2D>> ColorTargets;
		std::shared_ptr<RHITexture2D> DepthStencil;
		bool bBindDepthStencil = true;
		/** When set, binds via SetRenderTarget(RT, mip) and ignores ColorTargets (e.g. mip chain downsampling). */
		std::shared_ptr<RHIRenderTarget> ColorRenderTarget;
		int32_t ColorRenderTargetMipIndex = 0;
		int32_t ViewportOffsetX = 0;
		int32_t ViewportOffsetY = 0;
		/** Non-positive: derive full rect from first non-null color target (depth-only passes must set explicitly). */
		int32_t ViewportWidth = -1;
		int32_t ViewportHeight = -1;
		const char* DebugName = nullptr;
		/** Optional barrier list applied before SetRenderTarget (see FRDGAllSubresources). */
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

		if (Desc.ColorRenderTarget)
		{
			Ctx.SetRenderTarget(Desc.ColorRenderTarget, Desc.ColorRenderTargetMipIndex);
			int32_t W = Desc.ViewportWidth;
			int32_t H = Desc.ViewportHeight;
			if (W <= 0 || H <= 0)
			{
				if (std::shared_ptr<RHITexture2D> Tex = Desc.ColorRenderTarget->GetTex())
				{
					const core::vec2i Sz = Tex->GetSize();
					const int32_t mip = Desc.ColorRenderTargetMipIndex > 0 ? Desc.ColorRenderTargetMipIndex : 0;
					const int32_t DimX = Sz.x >> mip;
					const int32_t DimY = Sz.y >> mip;
					W = DimX > 0 ? DimX : 1;
					H = DimY > 0 ? DimY : 1;
				}
			}
			if (W > 0 && H > 0)
				Ctx.SetViewPort(Desc.ViewportOffsetX, Desc.ViewportOffsetY, W, H);
			return;
		}

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
