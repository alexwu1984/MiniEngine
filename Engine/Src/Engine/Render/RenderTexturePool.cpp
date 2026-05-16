#include "Render/RenderTexturePool.h"
#include "RHI/DynamicRHI.h"
#include "RHI/RHITexture2D.h"
#include "RHI/RHIUnorderedAccessView.h"
#include "RHI/RHIRenderTarget.h"
#include "RHI/RHI.h"
#include <algorithm>

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

	uint64_t RenderTexturePool::EstimateTextureBytes(EPixelFormat Format, int32_t W, int32_t H, uint32_t NumMips)
	{
		if (W <= 0 || H <= 0 || NumMips == 0)
			return 0;

		const FPixelFormatInfo& Info = GPixelFormats[Format];
		const int32_t BlockSizeX = std::max(Info.BlockSizeX, 1);
		const int32_t BlockSizeY = std::max(Info.BlockSizeY, 1);
		const int32_t BlockBytes = std::max(Info.BlockBytes, 0);
		if (BlockBytes == 0)
			return 0;

		uint64_t Total = 0;
		for (uint32_t Mip = 0; Mip < NumMips; ++Mip)
		{
			const int32_t MipW = std::max(1, W >> Mip);
			const int32_t MipH = std::max(1, H >> Mip);
			const uint64_t BlocksX = (uint64_t)(MipW + BlockSizeX - 1) / (uint64_t)BlockSizeX;
			const uint64_t BlocksY = (uint64_t)(MipH + BlockSizeY - 1) / (uint64_t)BlockSizeY;
			Total += BlocksX * BlocksY * (uint64_t)BlockBytes;
		}
		return Total;
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

	RenderTexturePool::Stats RenderTexturePool::GetStats() const
	{
		Stats S;
		S.FrameCounter = FrameCounter;
		S.FreeTex2D = CountCached(Tex2DFree);
		S.FreeUav = CountCached(UavFree);
		S.FreeRt = CountCached(RtFree);
		S.EstimatedBytesFree = EstimatedBytesFree;
		S.BudgetBytes = BudgetBytes;
		return S;
	}

	void RenderTexturePool::SetBudgetBytes(uint64_t InBudgetBytes)
	{
		BudgetBytes = InBudgetBytes;
	}

	uint64_t RenderTexturePool::GetBudgetBytes() const
	{
		return BudgetBytes;
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
			const uint64_t Bytes = EstimateTextureBytes(Format, Width, Height, NumMips);
			EstimatedBytesFree = (EstimatedBytesFree > Bytes) ? (EstimatedBytesFree - Bytes) : 0;
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
		{
			Bucket.push_back(PoolEntry<RHITexture2D>{ std::move(Tex), FrameCounter });
			EstimatedBytesFree += EstimateTextureBytes(Format, Width, Height, NumMips);
		}
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
			const uint64_t Bytes = EstimateTextureBytes(Format, Width, Height, 1);
			EstimatedBytesFree = (EstimatedBytesFree > Bytes) ? (EstimatedBytesFree - Bytes) : 0;
			return R;
		}

		return RHI->RHICreateUnorderedAccessViewForTransientPool(Format, Width, Height, false);
	}

	void RenderTexturePool::ReleaseUAV(
		EPixelFormat Format, int32_t Width, int32_t Height, std::shared_ptr<RHIUnorderedAccessView>&& Uav)
	{
		if (!Uav)
			return;

		const UavKey K{ Format, Width, Height };
		auto& Bucket = UavFree[K];
		if (Bucket.size() < kMaxFreePerKey)
		{
			Bucket.push_back(PoolEntry<RHIUnorderedAccessView>{ std::move(Uav), FrameCounter });
			EstimatedBytesFree += EstimateTextureBytes(Format, Width, Height, 1);
		}
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
			uint64_t Bytes = EstimateTextureBytes(Format, Width, Height, NumMips);
			if (CreateDepth)
				Bytes += EstimateTextureBytes(EPixelFormat::PF_DepthStencil, Width, Height, 1);
			if (IsMultiSampled)
				Bytes *= 2; // conservative: sample count isn't tracked in current API
			EstimatedBytesFree = (EstimatedBytesFree > Bytes) ? (EstimatedBytesFree - Bytes) : 0;
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
		{
			Bucket.push_back(PoolEntry<RHIRenderTarget>{ std::move(Rt), FrameCounter });
			uint64_t Bytes = EstimateTextureBytes(Format, Width, Height, NumMips);
			if (CreateDepth)
				Bytes += EstimateTextureBytes(EPixelFormat::PF_DepthStencil, Width, Height, 1);
			if (IsMultiSampled)
				Bytes *= 2; // conservative: sample count isn't tracked in current API
			EstimatedBytesFree += Bytes;
		}
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

		// Trim to a budget (approximate LRU) instead of clearing the entire pool.
		if (BudgetBytes > 0 && EstimatedBytesFree > BudgetBytes)
		{
			// Keep it simple and safe: repeatedly remove the globally-oldest entry (across pools),
			// based on each bucket's oldest element (drop globally oldest while over budget).
			while (EstimatedBytesFree > BudgetBytes)
			{
				// Find the currently oldest available entry among buckets.
				uint64_t BestFrame = UINT64_MAX;
				int BestKind = 0; // 0=Tex2D, 1=UAV, 2=RT
				bool Found = false;
				Tex2DKey BestTexK{};
				UavKey BestUavK{};
				RtKey BestRtK{};
				uint64_t BestBytes = 0;

				for (const auto& It : Tex2DFree)
				{
					const auto& Bucket = It.second;
					if (!Bucket.empty() && Bucket.front().LastUsedFrame < BestFrame)
					{
						BestFrame = Bucket.front().LastUsedFrame;
						BestKind = 0;
						BestTexK = It.first;
						BestBytes = EstimateTextureBytes(It.first.Format, It.first.W, It.first.H, It.first.NumMips);
						Found = true;
					}
				}
				for (const auto& It : UavFree)
				{
					const auto& Bucket = It.second;
					if (!Bucket.empty() && Bucket.front().LastUsedFrame < BestFrame)
					{
						BestFrame = Bucket.front().LastUsedFrame;
						BestKind = 1;
						BestUavK = It.first;
						BestBytes = EstimateTextureBytes(It.first.Format, It.first.W, It.first.H, 1);
						Found = true;
					}
				}
				for (const auto& It : RtFree)
				{
					const auto& Bucket = It.second;
					if (!Bucket.empty() && Bucket.front().LastUsedFrame < BestFrame)
					{
						BestFrame = Bucket.front().LastUsedFrame;
						BestKind = 2;
						BestRtK = It.first;
						BestBytes = EstimateTextureBytes(It.first.Format, It.first.W, It.first.H, It.first.NumMips);
						if (It.first.Depth)
							BestBytes += EstimateTextureBytes(EPixelFormat::PF_DepthStencil, It.first.W, It.first.H, 1);
						if (It.first.IsMS)
							BestBytes *= 2;
						Found = true;
					}
				}

				if (!Found)
					break;

				if (BestKind == 0)
				{
					auto It = Tex2DFree.find(BestTexK);
					if (It != Tex2DFree.end() && !It->second.empty())
					{
						It->second.erase(It->second.begin());
						if (It->second.empty())
							Tex2DFree.erase(It);
					}
				}
				else if (BestKind == 1)
				{
					auto It = UavFree.find(BestUavK);
					if (It != UavFree.end() && !It->second.empty())
					{
						It->second.erase(It->second.begin());
						if (It->second.empty())
							UavFree.erase(It);
					}
				}
				else
				{
					auto It = RtFree.find(BestRtK);
					if (It != RtFree.end() && !It->second.empty())
					{
						It->second.erase(It->second.begin());
						if (It->second.empty())
							RtFree.erase(It);
					}
				}

				EstimatedBytesFree = (EstimatedBytesFree > BestBytes) ? (EstimatedBytesFree - BestBytes) : 0;
			}
		}
	}

	void RenderTexturePool::Clear()
	{
		Tex2DFree.clear();
		UavFree.clear();
		RtFree.clear();
		EstimatedBytesFree = 0;
	}

	void RenderTexturePool::ApplyConfigFromJson(const nlohmann::json& Root)
	{
		try
		{
			if (Root.find("RenderTexturePool") == Root.end() || !Root["RenderTexturePool"].is_object())
				return;
			const auto& J = Root["RenderTexturePool"];
			if (J.find("BudgetBytes") != J.end() && J["BudgetBytes"].is_number_unsigned())
				SetBudgetBytes(J["BudgetBytes"].get<uint64_t>());
			else if (J.find("BudgetBytes") != J.end() && J["BudgetBytes"].is_number_integer())
				SetBudgetBytes(static_cast<uint64_t>(J["BudgetBytes"].get<int64_t>()));
			else if (J.find("BudgetMB") != J.end() && J["BudgetMB"].is_number())
			{
				const double Mb = J["BudgetMB"].get<double>();
				if (Mb >= 0.0)
					SetBudgetBytes(static_cast<uint64_t>(Mb * 1024.0 * 1024.0));
			}
		}
		catch (const std::exception&)
		{
		}
	}
}
