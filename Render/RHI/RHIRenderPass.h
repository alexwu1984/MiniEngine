#pragma once

/** Render-pass roadmap (RHIRenderPass + RDG): 0 OM discipline (PSO/OM consistency via FD3D12StateCache; no draw-path OM rebinding),
 *  1 FRHIRenderPass shell + optional inferred barriers (attachments + ShaderResourceReads SRV list; mip RT subresources),
 *  2 DeclaredTextureBarriers + DeclaredStructuredBufferBarriers,
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
		/** Optional structured-buffer transitions (D3D12); skipped for BUF_Dynamic UPLOAD buffers in the backend. */
		std::vector<FRDGStructuredBufferBarrierDesc> DeclaredStructuredBufferBarriers;
		/**
		 * When true (default), RHIBeginRenderPass prepends inferred RTV/DSV transitions for attachments in this desc.
		 * Uses per-mip subresource indices for ColorRenderTarget mip rendering; whole-resource for other attachments.
		 * Set false when pass barriers are fully specified in DeclaredTextureBarriers / RDG.
		 */
		bool bInferAttachmentBarriers = true;
		/**
		 * Non-null entries become FRDGTextureBarrierDesc SRV transitions (whole-resource) prepended after attachment inference.
		 * Use for pixel/compute textures sampled in-pass without wiring FRDGPassDescriptor.
		 */
		std::vector<std::shared_ptr<RHITexture2D>> ShaderResourceReads;
		bool bInferShaderResourceBarriers = true;

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

	inline void RHIRenderPassAppendInferredAttachmentBarriers(FRHIRenderPassDesc& Desc)
	{
		if (!Desc.bInferAttachmentBarriers)
			return;

		std::vector<FRDGTextureBarrierDesc> inferred;

		auto mipSubresourceSinglePlane2D = [](const RHITexture2D& Tex, int32_t mipSlice) -> uint32_t {
			uint32_t mips = Tex.GetNumMips();
			if (mips == 0u)
				mips = 1u;
			uint32_t m = mipSlice < 0 ? 0u : static_cast<uint32_t>(mipSlice);
			if (m >= mips)
				m = mips - 1u;
			return m;
		};

		if (Desc.ColorRenderTarget)
		{
			if (std::shared_ptr<RHITexture2D> tex = Desc.ColorRenderTarget->GetTex())
				inferred.push_back({ tex, FRDGResourceAccess::RTV, mipSubresourceSinglePlane2D(*tex, Desc.ColorRenderTargetMipIndex), {} });
		}
		else
		{
			for (const auto& ct : Desc.ColorTargets)
			{
				if (ct)
					inferred.push_back({ ct, FRDGResourceAccess::RTV, FRDGAllSubresources, {} });
			}
			if (Desc.bBindDepthStencil && Desc.DepthStencil)
				inferred.push_back({ Desc.DepthStencil, FRDGResourceAccess::DSV, FRDGAllSubresources, {} });
		}

		if (inferred.empty())
			return;

		std::vector<FRDGTextureBarrierDesc> merged;
		merged.reserve(inferred.size() + Desc.DeclaredTextureBarriers.size());
		merged.insert(merged.end(), inferred.begin(), inferred.end());
		for (FRDGTextureBarrierDesc& B : Desc.DeclaredTextureBarriers)
			merged.push_back(std::move(B));
		Desc.DeclaredTextureBarriers = std::move(merged);
	}

	inline void RHIRenderPassAppendInferredShaderResourceBarriers(FRHIRenderPassDesc& Desc)
	{
		if (!Desc.bInferShaderResourceBarriers)
			return;

		std::vector<FRDGTextureBarrierDesc> inferred;
		for (const std::shared_ptr<RHITexture2D>& tr : Desc.ShaderResourceReads)
			if (tr)
				inferred.push_back({ tr, FRDGResourceAccess::SRV, FRDGAllSubresources, {} });

		if (inferred.empty())
			return;

		std::vector<FRDGTextureBarrierDesc> merged;
		merged.reserve(inferred.size() + Desc.DeclaredTextureBarriers.size());
		merged.insert(merged.end(), inferred.begin(), inferred.end());
		for (FRDGTextureBarrierDesc& B : Desc.DeclaredTextureBarriers)
			merged.push_back(std::move(B));
		Desc.DeclaredTextureBarriers = std::move(merged);
	}

	inline void RHIBeginRenderPass(RHICommandContext& Ctx, FRHIRenderPassDesc Desc)
	{
		RHIRenderPassAppendInferredAttachmentBarriers(Desc);
		RHIRenderPassAppendInferredShaderResourceBarriers(Desc);

		if (Desc.DebugName)
			Ctx.BeginUserMark(Desc.DebugName);

		if (!Desc.DeclaredTextureBarriers.empty())
			Ctx.RHIRenderPassApplyDeclaredTextureBarriers(Desc.DeclaredTextureBarriers.data(), Desc.DeclaredTextureBarriers.size(), ERDGPassQueue::Graphics);

		if (!Desc.DeclaredStructuredBufferBarriers.empty())
			Ctx.RHIRenderPassApplyDeclaredStructuredBufferBarriers(Desc.DeclaredStructuredBufferBarriers.data(), Desc.DeclaredStructuredBufferBarriers.size(),
																   ERDGPassQueue::Graphics);

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
