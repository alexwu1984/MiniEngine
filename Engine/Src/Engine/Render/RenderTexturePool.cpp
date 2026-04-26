#include "Render/RenderTexturePool.h"
#include "RHI/DynamicRHI.h"
#include "RHI/RHITexture2D.h"
#include "RHI/RHIUnorderedAccessView.h"
#include "RHI/RHIRenderTarget.h"

namespace Engine
{
	using namespace RenderCore;

	namespace
	{
		template <class TKey, class TValue>
		std::size_t CountCached(const std::map<TKey, std::vector<RenderTexturePool::PoolEntry<TValue>>>& M)
		{
			std::size_t Total = 0;
			for (const auto& It : M)
				Total += It.second.size();
			return Total;
		}

		template <class TKey, class TValue>
		void EvictOld(std::map<TKey, std::vector<RenderTexturePool::PoolEntry<TValue>>>& M, uint64_t FrameCounter)
		{
			for (auto It = M.begin(); It != M.end();)
			{
				auto& Bucket = It->second;
				for (auto B = Bucket.begin(); B != Bucket.end();)
				{
					const uint64_t Age = FrameCounter - B->LastUsedFrame;
					if (Age > RenderTexturePool::kEvictAfterFrames)
						B = Bucket.erase(B);
					else
						++B;
				}
				if (Bucket.empty())
					It = M.erase(It);
				else
					++It;
			}
		}
	}

	bool RenderTexturePool::Tex2DKey::operator<(const Tex2DKey& o) const
	{
		if (Format != o.Format)
			return Format < o.Format;
		if (Flags != o.Flags)
			return Flags < o.Flags;
		if (W != o.W)
			return W < o.W;
		if (H != o.H)
			return H < o.H;
		return NumMips < o.NumMips;
	}

	bool RenderTexturePool::UavKey::operator<(const UavKey& o) const
	{
		if (Format != o.Format)
			return Format < o.Format;
		if (W != o.W)
			return W < o.W;
		return H < o.H;
	}

	bool RenderTexturePool::RtKey::operator<(const RtKey& o) const
	{
		if (Format != o.Format)
			return Format < o.Format;
		if (W != o.W)
			return W < o.W;
		if (H != o.H)
			return H < o.H;
		if (NumMips != o.NumMips)
			return NumMips < o.NumMips;
		if (IsMS != o.IsMS)
			return IsMS < o.IsMS;
		return Depth < o.Depth;
	}

	RenderTexturePool& RenderTexturePool::Get()
	{
		static RenderTexturePool Instance;
		return Instance;
	}

	std::shared_ptr<RHITexture2D> RenderTexturePool::AcquireTexture2D(
		DynamicRHI* RHI, EPixelFormat Format, int32_t CreateFlags, int32_t Width, int32_t Height, uint32_t NumMips)
	{
		if (!RHI || Width <= 0 || Height <= 0)
			return {};

		const Tex2DKey K{ Format, CreateFlags, Width, Height, NumMips };
		auto& Bucket = Tex2DFree[K];
		if (!Bucket.empty())
		{
			auto R = std::move(Bucket.back().Resource);
			Bucket.pop_back();
			if (Bucket.empty())
				Tex2DFree.erase(K);
			return R;
		}

		return RHI->RHICreateTexture2D(Format, CreateFlags, Width, Height, NumMips);
	}

	void RenderTexturePool::ReleaseTexture2D(
		EPixelFormat Format, int32_t CreateFlags, int32_t Width, int32_t Height, uint32_t NumMips,
		std::shared_ptr<RHITexture2D>&& Tex)
	{
		if (!Tex)
			return;

		const Tex2DKey K{ Format, CreateFlags, Width, Height, NumMips };
		auto& Bucket = Tex2DFree[K];
		if (Bucket.size() < kMaxFreePerKey)
			Bucket.push_back(PoolEntry<RHITexture2D>{ std::move(Tex), FrameCounter });
	}

	std::shared_ptr<RHIUnorderedAccessView> RenderTexturePool::AcquireUAV(
		DynamicRHI* RHI, EPixelFormat Format, int32_t Width, int32_t Height)
	{
		if (!RHI || Width <= 0 || Height <= 0)
			return {};

		const UavKey K{ Format, Width, Height };
		auto& Bucket = UavFree[K];
		if (!Bucket.empty())
		{
			auto R = std::move(Bucket.back().Resource);
			Bucket.pop_back();
			if (Bucket.empty())
				UavFree.erase(K);
			return R;
		}

		return RHI->RHICreateUnorderedAccessView(Format, Width, Height);
	}

	void RenderTexturePool::ReleaseUAV(
		EPixelFormat Format, int32_t Width, int32_t Height, std::shared_ptr<RHIUnorderedAccessView>&& Uav)
	{
		if (!Uav)
			return;

		const UavKey K{ Format, Width, Height };
		auto& Bucket = UavFree[K];
		if (Bucket.size() < kMaxFreePerKey)
			Bucket.push_back(PoolEntry<RHIUnorderedAccessView>{ std::move(Uav), FrameCounter });
	}

	std::shared_ptr<RHIRenderTarget> RenderTexturePool::AcquireRenderTarget(
		DynamicRHI* RHI, EPixelFormat Format, int32_t Width, int32_t Height, uint32_t NumMips,
		bool IsMultiSampled, bool CreateDepth)
	{
		if (!RHI || Width <= 0 || Height <= 0)
			return {};

		const RtKey K{ Format, Width, Height, NumMips, IsMultiSampled, CreateDepth };
		auto& Bucket = RtFree[K];
		if (!Bucket.empty())
		{
			auto R = std::move(Bucket.back().Resource);
			Bucket.pop_back();
			if (Bucket.empty())
				RtFree.erase(K);
			return R;
		}

		return RHI->RHICreateRenderTarget(Format, Width, Height, NumMips, IsMultiSampled, CreateDepth);
	}

	void RenderTexturePool::ReleaseRenderTarget(EPixelFormat Format, int32_t Width, int32_t Height, uint32_t NumMips,
		bool IsMultiSampled, bool CreateDepth, std::shared_ptr<RHIRenderTarget>&& Rt)
	{
		if (!Rt)
			return;

		const RtKey K{ Format, Width, Height, NumMips, IsMultiSampled, CreateDepth };
		auto& Bucket = RtFree[K];
		if (Bucket.size() < kMaxFreePerKey)
			Bucket.push_back(PoolEntry<RHIRenderTarget>{ std::move(Rt), FrameCounter });
	}

	void RenderTexturePool::BeginFrame()
	{
		++FrameCounter;
	}

	void RenderTexturePool::EndFrame()
	{
		// Evict entries unused for a while to avoid slow growth and keep memory stable.
		EvictOld(Tex2DFree, FrameCounter);
		EvictOld(UavFree, FrameCounter);
		EvictOld(RtFree, FrameCounter);

		// Safety trim: if resolution fluctuates (or effects allocate many unique sizes),
		// the key maps can grow unbounded. This keeps runtime memory stable.
		static constexpr std::size_t kMaxCachedTextures = 256;
		static constexpr std::size_t kMaxCachedUavs = 256;
		static constexpr std::size_t kMaxCachedRenderTargets = 128;

		const std::size_t CachedTex = CountCached(Tex2DFree);
		const std::size_t CachedUav = CountCached(UavFree);
		const std::size_t CachedRt = CountCached(RtFree);

		if (CachedTex > kMaxCachedTextures || CachedUav > kMaxCachedUavs || CachedRt > kMaxCachedRenderTargets)
		{
			Clear();
		}
	}

	void RenderTexturePool::Clear()
	{
		Tex2DFree.clear();
		UavFree.clear();
		RtFree.clear();
	}
}
