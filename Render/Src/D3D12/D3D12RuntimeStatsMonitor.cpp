#include "D3D12/D3D12RuntimeStatsMonitor.h"

#include "D3D12/D3D12Adapter.h"
#include "D3D12/D3D12CommandContext.h"
#include "D3D12/D3D12WindowDevice.h"
#include "D3D12/D3D12CreateStats.h"
#include "D3D12/D3D12MemoryMonitor.h"
#include "D3D12/D3D12PresentStats.h"
#include "D3D12/D3D12Resource.h"
#include "D3D12/D3D12StateCache.h"
#include "D3D12/D3D12SubmitStats.h"
#include "D3D12/D3D12RHI.h"

#include "../../../ThirdParty/DirectXTex/DXTexStats.h"

#include "core/commandline.h"
#include "core/logger.h"

#include <windows.h>
#include <dxgi1_4.h>
#include <mutex>

namespace RenderCore
{
	namespace
	{
		static bool TickGateOncePerSecond()
		{
			static ULONGLONG sLastTick = 0;
			const ULONGLONG now = ::GetTickCount64();
			if (sLastTick == 0)
				sLastTick = now;
			if (now - sLastTick < 1000)
				return false;
			sLastTick = now;
			return true;
		}
	}

	void D3D12RuntimeStatsMonitor::TickOncePerSecond(D3D12CommandContext& Context,
		const std::shared_ptr<FD3D12Adapter>& Adapter,
		const std::shared_ptr<FD3D12Device>& Device)
	{
		if (!core::CommandLine::Get().GetName("d3d12_memmon"))
			return;
		if (!TickGateOncePerSecond())
			return;
		if (!Device)
			return;

		const auto Pools = Device->GetDynamicDescriptorHeapPools();
		const std::size_t DynViewHeaps = Pools.CreatedTracking[0].size();
		const std::size_t DynSamplerHeaps = Pools.CreatedTracking[1].size();

		const auto CpuW = Device->GetLinearPageManager(ELinearAllocatorType::CpuWritable);
		const auto GpuX = Device->GetLinearPageManager(ELinearAllocatorType::GpuExclusive);

		std::size_t RS = 0, GPSO = 0, CPSO = 0;
		if (Context.CurrentStateCache)
		{
			RS = Context.CurrentStateCache->GetRootSignatureCacheSize();
			GPSO = Context.CurrentStateCache->GetGraphicsPSOCacheSize();
			CPSO = Context.CurrentStateCache->GetComputePSOCacheSize();
		}

		std::size_t TexCaches = 0, VS = 0, PS = 0, CS = 0;
		if (Adapter)
		{
			if (auto Rhi = std::dynamic_pointer_cast<D3D12DynamicRHI>(Adapter->GetOwningRHI()))
			{
				const auto Stats = Rhi->GetCacheStats();
				TexCaches = Stats.TexCaches;
				VS = Stats.VS;
				PS = Stats.PS;
				CS = Stats.CS;
			}
		}

		static uint64_t sFrame = 0;
		++sFrame;

		core::LOG(core::log_inf,
			L"[D3D12] Frame=%llu DynHeaps(View=%zu Sampler=%zu) Pages(CpuStd=%zu CpuLarge=%zu CpuRet=%zu GpuStd=%zu GpuLarge=%zu GpuRet=%zu)",
			(unsigned long long)sFrame,
			DynViewHeaps, DynSamplerHeaps,
			CpuW.GetStandardPageCount(), CpuW.GetLargePageCount(), CpuW.GetRetiredPageCount(),
			GpuX.GetStandardPageCount(), GpuX.GetLargePageCount(), GpuX.GetRetiredPageCount());

		core::LOG(core::log_inf,
			L"[D3D12] LinearPagePools CpuWritable(owned=%zu ready=%zu retired=%zu/%zu/%zu) GpuExclusive(owned=%zu ready=%zu retired=%zu/%zu/%zu)",
			CpuW.GetStandardPageCount(), CpuW.GetReadyPageCount(),
			CpuW.GetRetiredPageCountForQueue(0), CpuW.GetRetiredPageCountForQueue(1), CpuW.GetRetiredPageCountForQueue(2),
			GpuX.GetStandardPageCount(), GpuX.GetReadyPageCount(),
			GpuX.GetRetiredPageCountForQueue(0), GpuX.GetRetiredPageCountForQueue(1), GpuX.GetRetiredPageCountForQueue(2));

		core::LOG(core::log_inf,
			L"[D3D12] DXTex Calls Capture=%llu SaveWIC=%llu SaveDDS=%llu LoadWIC=%llu LoadDDS=%llu",
			(unsigned long long)DXTexStats::CaptureTextureCalls_D3D12().load(std::memory_order_relaxed),
			(unsigned long long)DXTexStats::ScreenGrab_SaveWICCalls_D3D12().load(std::memory_order_relaxed),
			(unsigned long long)DXTexStats::ScreenGrab_SaveDDSCalls_D3D12().load(std::memory_order_relaxed),
			(unsigned long long)DXTexStats::WICTextureLoader_LoadFromFileCalls_D3D12().load(std::memory_order_relaxed),
			(unsigned long long)DXTexStats::DDSTextureLoader_LoadFromFileCalls_D3D12().load(std::memory_order_relaxed));

		core::LOG(core::log_inf,
			L"[D3D12] CpuDescHeaps Pool=%zu CBVSRVUAV(Heaps=%zu Free=%zu) Sampler(Heaps=%zu Free=%zu) RTV(Heaps=%zu Free=%zu) DSV(Heaps=%zu Free=%zu)",
			Device->GetCpuDescriptorGlobalPoolSize(),
			Device->GetCpuDescriptorHeapCount(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV), Device->GetCpuDescriptorFreeBlockCount(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV),
			Device->GetCpuDescriptorHeapCount(D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER), Device->GetCpuDescriptorFreeBlockCount(D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER),
			Device->GetCpuDescriptorHeapCount(D3D12_DESCRIPTOR_HEAP_TYPE_RTV), Device->GetCpuDescriptorFreeBlockCount(D3D12_DESCRIPTOR_HEAP_TYPE_RTV),
			Device->GetCpuDescriptorHeapCount(D3D12_DESCRIPTOR_HEAP_TYPE_DSV), Device->GetCpuDescriptorFreeBlockCount(D3D12_DESCRIPTOR_HEAP_TYPE_DSV));

		core::LOG(core::log_inf,
			L"[D3D12] Cache RS=%zu PSO(G=%zu C=%zu) Shaders(VS=%zu PS=%zu CS=%zu) TexCaches=%zu",
			RS, GPSO, CPSO, VS, PS, CS, TexCaches);

		{
			const auto Live = FD3D12Resource::GetLiveStats();
			const double MB = 1024.0 * 1024.0;
			core::LOG(core::log_inf,
				L"[D3D12] LiveRes Default(Cnt=%llu Bytes=%.1fMB) Upload(Cnt=%llu Bytes=%.1fMB) Readback(Cnt=%llu Bytes=%.1fMB)",
				(unsigned long long)Live.DefaultCount, (double)Live.DefaultBytes / MB,
				(unsigned long long)Live.UploadCount, (double)Live.UploadBytes / MB,
				(unsigned long long)Live.ReadbackCount, (double)Live.ReadbackBytes / MB);

			static uint64_t sPrevCreateCnt = 0, sPrevDestroyCnt = 0, sPrevCreateBytes = 0, sPrevDestroyBytes = 0;
			const uint64_t dCreateCnt = Live.TotalCreateCount - sPrevCreateCnt;
			const uint64_t dDestroyCnt = Live.TotalDestroyCount - sPrevDestroyCnt;
			const uint64_t dCreateBytes = Live.TotalCreateBytes - sPrevCreateBytes;
			const uint64_t dDestroyBytes = Live.TotalDestroyBytes - sPrevDestroyBytes;
			sPrevCreateCnt = Live.TotalCreateCount;
			sPrevDestroyCnt = Live.TotalDestroyCount;
			sPrevCreateBytes = Live.TotalCreateBytes;
			sPrevDestroyBytes = Live.TotalDestroyBytes;

			core::LOG(core::log_inf,
				L"[D3D12] ResChurn +Create=%llu (%.1fMB) +Destroy=%llu (%.1fMB)",
				(unsigned long long)dCreateCnt, (double)dCreateBytes / MB,
				(unsigned long long)dDestroyCnt, (double)dDestroyBytes / MB);
		}

		{
			const double MB = 1024.0 * 1024.0;
			static uint64_t sPrevCpuPageUploadCnt = 0, sPrevCpuPageUploadBytes = 0;
			static uint64_t sPrevGpuPageDefaultCnt = 0, sPrevGpuPageDefaultBytes = 0;

			const uint64_t curUploadCnt = D3D12CreateStats::LinearPage_CreateCount_Upload().load(std::memory_order_relaxed);
			const uint64_t curUploadBytes = D3D12CreateStats::LinearPage_CreateBytes_Upload().load(std::memory_order_relaxed);
			const uint64_t curDefaultCnt = D3D12CreateStats::LinearPage_CreateCount_Default().load(std::memory_order_relaxed);
			const uint64_t curDefaultBytes = D3D12CreateStats::LinearPage_CreateBytes_Default().load(std::memory_order_relaxed);

			const uint64_t dUploadCnt = curUploadCnt - sPrevCpuPageUploadCnt;
			const uint64_t dUploadBytes = curUploadBytes - sPrevCpuPageUploadBytes;
			const uint64_t dDefaultCnt = curDefaultCnt - sPrevGpuPageDefaultCnt;
			const uint64_t dDefaultBytes = curDefaultBytes - sPrevGpuPageDefaultBytes;

			sPrevCpuPageUploadCnt = curUploadCnt;
			sPrevCpuPageUploadBytes = curUploadBytes;
			sPrevGpuPageDefaultCnt = curDefaultCnt;
			sPrevGpuPageDefaultBytes = curDefaultBytes;

			core::LOG(core::log_inf,
				L"[D3D12] DirectCreates LinearPages Upload(+%llu %.1fMB) Default(+%llu %.1fMB)",
				(unsigned long long)dUploadCnt, (double)dUploadBytes / MB,
				(unsigned long long)dDefaultCnt, (double)dDefaultBytes / MB);
		}

		{
			auto& DirectMgr = Device->GetCommandListManager(ED3D12CommandQueueType::Default);
			auto& CopyMgr = Device->GetCommandListManager(ED3D12CommandQueueType::Copy);
			auto& AsyncMgr = Device->GetCommandListManager(ED3D12CommandQueueType::Async);

			core::LOG(core::log_inf,
				L"[D3D12] CmdLists Ready(Direct=%u Copy=%u Async=%u)",
				DirectMgr.GetReadyListCount(), CopyMgr.GetReadyListCount(), AsyncMgr.GetReadyListCount());

			static uint64_t sPrevSubmitDirect = 0, sPrevSubmitCopy = 0, sPrevSubmitCompute = 0;
			const D3D12SubmitStats::Snapshot s = D3D12SubmitStats::GetSnapshot();
			const uint64_t curSubmitDirect = s.Direct;
			const uint64_t curSubmitCopy = s.Copy;
			const uint64_t curSubmitCompute = s.Compute;
			const uint64_t dSubmitDirect = curSubmitDirect - sPrevSubmitDirect;
			const uint64_t dSubmitCopy = curSubmitCopy - sPrevSubmitCopy;
			const uint64_t dSubmitCompute = curSubmitCompute - sPrevSubmitCompute;
			sPrevSubmitDirect = curSubmitDirect;
			sPrevSubmitCopy = curSubmitCopy;
			sPrevSubmitCompute = curSubmitCompute;

			core::LOG(core::log_inf,
				L"[D3D12] Submits +Direct=%llu +Copy=%llu +Async=%llu",
				(unsigned long long)dSubmitDirect,
				(unsigned long long)dSubmitCopy,
				(unsigned long long)dSubmitCompute);

			static uint64_t sPrevPresentCalls = 0, sPrevPresentOcc = 0, sPrevPresentFail = 0, sPrevIconic = 0, sPrevNotVis = 0;
			const uint64_t curPresentCalls = D3D12PresentStats::PresentCalls().load(std::memory_order_relaxed);
			const uint64_t curPresentOcc = D3D12PresentStats::PresentOccluded().load(std::memory_order_relaxed);
			const uint64_t curPresentFail = D3D12PresentStats::PresentFailed().load(std::memory_order_relaxed);
			const uint64_t curIconic = D3D12PresentStats::WindowIconic().load(std::memory_order_relaxed);
			const uint64_t curNotVis = D3D12PresentStats::WindowNotVisible().load(std::memory_order_relaxed);
			const uint64_t dPresentCalls = curPresentCalls - sPrevPresentCalls;
			const uint64_t dPresentOcc = curPresentOcc - sPrevPresentOcc;
			const uint64_t dPresentFail = curPresentFail - sPrevPresentFail;
			const uint64_t dIconic = curIconic - sPrevIconic;
			const uint64_t dNotVis = curNotVis - sPrevNotVis;
			sPrevPresentCalls = curPresentCalls;
			sPrevPresentOcc = curPresentOcc;
			sPrevPresentFail = curPresentFail;
			sPrevIconic = curIconic;
			sPrevNotVis = curNotVis;

			core::LOG(core::log_inf,
				L"[D3D12] Present +Calls=%llu +Occ=%llu +Fail=%llu Win(+Iconic=%llu +NotVis=%llu)",
				(unsigned long long)dPresentCalls,
				(unsigned long long)dPresentOcc,
				(unsigned long long)dPresentFail,
				(unsigned long long)dIconic,
				(unsigned long long)dNotVis);

			core::LOG(core::log_inf,
				L"[D3D12] CmdAllocs(Context Total=%zu Available=%zu)",
				Context.CommandAllocatorManager.GetTotalAllocatorCount(),
				Context.CommandAllocatorManager.GetAvailableAllocatorCount());
		}

		if (!Adapter)
			return;

		{
			auto& FrameFence = Adapter->GetFrameFence();
			core::LOG(core::log_inf,
				L"[D3D12] Fence Frame(Cur=%llu Sig=%llu CompFast=%llu CompPeek=%llu)",
				(unsigned long long)FrameFence.GetCurrentFence(),
				(unsigned long long)FrameFence.GetLastSignaledFence(),
				(unsigned long long)FrameFence.GetLastCompletedFenceFast(),
				(unsigned long long)FrameFence.PeekLastCompletedFence());

			auto& DirectMgr = Device->GetCommandListManager(ED3D12CommandQueueType::Default);
			auto& CopyMgr = Device->GetCommandListManager(ED3D12CommandQueueType::Copy);
			auto& AsyncMgr = Device->GetCommandListManager(ED3D12CommandQueueType::Async);
			auto& DirectFence = DirectMgr.GetFence();
			auto& CopyFence = CopyMgr.GetFence();
			auto& AsyncFence = AsyncMgr.GetFence();
			core::LOG(core::log_inf,
				L"[D3D12] Fence Queue(Direct Cur=%llu Sig=%llu CompFast=%llu CompPeek=%llu) "
				L"(Copy Cur=%llu Sig=%llu CompFast=%llu CompPeek=%llu) "
				L"(Async Cur=%llu Sig=%llu CompFast=%llu CompPeek=%llu)",
				(unsigned long long)DirectFence.GetCurrentFence(),
				(unsigned long long)DirectFence.GetLastSignaledFence(),
				(unsigned long long)DirectFence.GetLastCompletedFenceFast(),
				(unsigned long long)DirectFence.PeekLastCompletedFence(),
				(unsigned long long)CopyFence.GetCurrentFence(),
				(unsigned long long)CopyFence.GetLastSignaledFence(),
				(unsigned long long)CopyFence.GetLastCompletedFenceFast(),
				(unsigned long long)CopyFence.PeekLastCompletedFence(),
				(unsigned long long)AsyncFence.GetCurrentFence(),
				(unsigned long long)AsyncFence.GetLastSignaledFence(),
				(unsigned long long)AsyncFence.GetLastCompletedFenceFast(),
				(unsigned long long)AsyncFence.PeekLastCompletedFence());
		}

		{
			auto& FencePool = Adapter->GetFenceCorePool();
			core::LOG(core::log_inf,
				L"[D3D12] Fences CorePool(TotalCreated=%llu Available=%u)",
				(unsigned long long)FencePool.GetTotalCreatedCount(),
				FencePool.GetAvailableCount());
		}
	}
}

