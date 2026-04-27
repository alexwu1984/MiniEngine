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
#include "D3D12/D3D12UploadWCDiagnostics.h"
#include "D3D12/D3D12RHI.h"
#include "D3D12/D3D12Allocation.h"
#include "RHI/RHI.h"

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
		if (!RenderCore::D3D12RHI_ShouldEnableMemMon())
			return;
		if (!TickGateOncePerSecond())
			return;
		if (!Device)
			return;

		const auto Pools = Device->GetDynamicDescriptorHeapPools();
		const std::size_t DynViewHeaps = Pools.CreatedTracking[0].size();
		const std::size_t DynSamplerHeaps = Pools.CreatedTracking[1].size();

		auto& UploadPool = Device->GetFastAllocator(UploadFastAllocator);
		auto& DefaultPool = Device->GetFastAllocator(DefaultFastAllocator);

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
			L"[D3D12] Frame=%llu DynHeaps(View=%zu Sampler=%zu) Pages(UploadStd=%zu UploadLarge=%zu UploadRet=%zu DefaultStd=%zu DefaultLarge=%zu DefaultRet=%zu)",
			(unsigned long long)sFrame,
			DynViewHeaps, DynSamplerHeaps,
			UploadPool.GetStandardPageCount(), UploadPool.GetLargePageCount(), UploadPool.GetRetiredPageCount(),
			DefaultPool.GetStandardPageCount(), DefaultPool.GetLargePageCount(), DefaultPool.GetRetiredPageCount());

		{
			static uint64_t sPrevCreateView = 0, sPrevCreateSampler = 0;
			static uint64_t sPrevReadyView = 0, sPrevReadySampler = 0;
			static uint64_t sPrevWaitView = 0, sPrevWaitSampler = 0;
			static uint64_t sPrevCopyCallsView = 0, sPrevCopyCallsSampler = 0;
			static uint64_t sPrevCopyDescView = 0, sPrevCopyDescSampler = 0;

			const uint64_t cCreateView = D3D12CreateStats::DynDesc_CreateCount_CbvSrvUav().load(std::memory_order_relaxed);
			const uint64_t cCreateSampler = D3D12CreateStats::DynDesc_CreateCount_Sampler().load(std::memory_order_relaxed);
			const uint64_t cReadyView = D3D12CreateStats::DynDesc_RecycleReadyCount_CbvSrvUav().load(std::memory_order_relaxed);
			const uint64_t cReadySampler = D3D12CreateStats::DynDesc_RecycleReadyCount_Sampler().load(std::memory_order_relaxed);
			const uint64_t cWaitView = D3D12CreateStats::DynDesc_FenceWaitReuseCount_CbvSrvUav().load(std::memory_order_relaxed);
			const uint64_t cWaitSampler = D3D12CreateStats::DynDesc_FenceWaitReuseCount_Sampler().load(std::memory_order_relaxed);
			const uint64_t cCopyCallsView = D3D12CreateStats::DynDesc_CopyDescriptorsCalls_CbvSrvUav().load(std::memory_order_relaxed);
			const uint64_t cCopyCallsSampler = D3D12CreateStats::DynDesc_CopyDescriptorsCalls_Sampler().load(std::memory_order_relaxed);
			const uint64_t cCopyDescView = D3D12CreateStats::DynDesc_CopyDescriptorsCount_CbvSrvUav().load(std::memory_order_relaxed);
			const uint64_t cCopyDescSampler = D3D12CreateStats::DynDesc_CopyDescriptorsCount_Sampler().load(std::memory_order_relaxed);

			core::LOG(core::log_inf,
				L"[D3D12] DynDesc/s Create(View=%llu Sampler=%llu) ReuseReady(View=%llu Sampler=%llu) WaitReuse(View=%llu Sampler=%llu) CopyCalls(View=%llu Sampler=%llu) CopyDesc(View=%llu Sampler=%llu)",
				(unsigned long long)(cCreateView - sPrevCreateView), (unsigned long long)(cCreateSampler - sPrevCreateSampler),
				(unsigned long long)(cReadyView - sPrevReadyView), (unsigned long long)(cReadySampler - sPrevReadySampler),
				(unsigned long long)(cWaitView - sPrevWaitView), (unsigned long long)(cWaitSampler - sPrevWaitSampler),
				(unsigned long long)(cCopyCallsView - sPrevCopyCallsView), (unsigned long long)(cCopyCallsSampler - sPrevCopyCallsSampler),
				(unsigned long long)(cCopyDescView - sPrevCopyDescView), (unsigned long long)(cCopyDescSampler - sPrevCopyDescSampler));

			sPrevCreateView = cCreateView; sPrevCreateSampler = cCreateSampler;
			sPrevReadyView = cReadyView; sPrevReadySampler = cReadySampler;
			sPrevWaitView = cWaitView; sPrevWaitSampler = cWaitSampler;
			sPrevCopyCallsView = cCopyCallsView; sPrevCopyCallsSampler = cCopyCallsSampler;
			sPrevCopyDescView = cCopyDescView; sPrevCopyDescSampler = cCopyDescSampler;
		}

		{
			static uint64_t sPrevCLReadyDirect = 0, sPrevCLCreateDirect = 0;
			static uint64_t sPrevCLReadyCompute = 0, sPrevCLCreateCompute = 0;

			const uint64_t cReadyDirect = D3D12CreateStats::CmdList_ObtainFromReadyCount_Direct().load(std::memory_order_relaxed);
			const uint64_t cCreateDirect = D3D12CreateStats::CmdList_CreateCount_Direct().load(std::memory_order_relaxed);
			const uint64_t cReadyCompute = D3D12CreateStats::CmdList_ObtainFromReadyCount_Compute().load(std::memory_order_relaxed);
			const uint64_t cCreateCompute = D3D12CreateStats::CmdList_CreateCount_Compute().load(std::memory_order_relaxed);

			core::LOG(core::log_inf,
				L"[D3D12] CmdList/s ObtainReady(Direct=%llu Compute=%llu) CreateNew(Direct=%llu Compute=%llu)",
				(unsigned long long)(cReadyDirect - sPrevCLReadyDirect),
				(unsigned long long)(cReadyCompute - sPrevCLReadyCompute),
				(unsigned long long)(cCreateDirect - sPrevCLCreateDirect),
				(unsigned long long)(cCreateCompute - sPrevCLCreateCompute));

			sPrevCLReadyDirect = cReadyDirect; sPrevCLCreateDirect = cCreateDirect;
			sPrevCLReadyCompute = cReadyCompute; sPrevCLCreateCompute = cCreateCompute;
		}

		{
			static uint64_t sPrevExecD = 0, sPrevUserD = 0, sPrevBarD = 0, sPrevTotalD = 0;
			static uint64_t sPrevExecC = 0, sPrevUserC = 0, sPrevBarC = 0, sPrevTotalC = 0;
			static uint64_t sPrevExecP = 0, sPrevUserP = 0, sPrevBarP = 0, sPrevTotalP = 0;

			const uint64_t cExecD = D3D12CreateStats::Submit_ExecCalls_Direct().load(std::memory_order_relaxed);
			const uint64_t cUserD = D3D12CreateStats::Submit_UserCLCount_Direct().load(std::memory_order_relaxed);
			const uint64_t cBarD = D3D12CreateStats::Submit_BarrierCLCount_Direct().load(std::memory_order_relaxed);
			const uint64_t cTotD = D3D12CreateStats::Submit_TotalCLCount_Direct().load(std::memory_order_relaxed);

			const uint64_t cExecC = D3D12CreateStats::Submit_ExecCalls_Compute().load(std::memory_order_relaxed);
			const uint64_t cUserC = D3D12CreateStats::Submit_UserCLCount_Compute().load(std::memory_order_relaxed);
			const uint64_t cBarC = D3D12CreateStats::Submit_BarrierCLCount_Compute().load(std::memory_order_relaxed);
			const uint64_t cTotC = D3D12CreateStats::Submit_TotalCLCount_Compute().load(std::memory_order_relaxed);

			const uint64_t cExecP = D3D12CreateStats::Submit_ExecCalls_Copy().load(std::memory_order_relaxed);
			const uint64_t cUserP = D3D12CreateStats::Submit_UserCLCount_Copy().load(std::memory_order_relaxed);
			const uint64_t cBarP = D3D12CreateStats::Submit_BarrierCLCount_Copy().load(std::memory_order_relaxed);
			const uint64_t cTotP = D3D12CreateStats::Submit_TotalCLCount_Copy().load(std::memory_order_relaxed);

			core::LOG(core::log_inf,
				L"[D3D12] SubmitCL/s Direct(exec=%llu user=%llu barrier=%llu total=%llu) Compute(exec=%llu user=%llu barrier=%llu total=%llu) Copy(exec=%llu user=%llu barrier=%llu total=%llu)",
				(unsigned long long)(cExecD - sPrevExecD),
				(unsigned long long)(cUserD - sPrevUserD),
				(unsigned long long)(cBarD - sPrevBarD),
				(unsigned long long)(cTotD - sPrevTotalD),
				(unsigned long long)(cExecC - sPrevExecC),
				(unsigned long long)(cUserC - sPrevUserC),
				(unsigned long long)(cBarC - sPrevBarC),
				(unsigned long long)(cTotC - sPrevTotalC),
				(unsigned long long)(cExecP - sPrevExecP),
				(unsigned long long)(cUserP - sPrevUserP),
				(unsigned long long)(cBarP - sPrevBarP),
				(unsigned long long)(cTotP - sPrevTotalP));

			sPrevExecD = cExecD; sPrevUserD = cUserD; sPrevBarD = cBarD; sPrevTotalD = cTotD;
			sPrevExecC = cExecC; sPrevUserC = cUserC; sPrevBarC = cBarC; sPrevTotalC = cTotC;
			sPrevExecP = cExecP; sPrevUserP = cUserP; sPrevBarP = cBarP; sPrevTotalP = cTotP;
		}

		core::LOG(core::log_inf,
			L"[D3D12] FastAllocatorPools Upload(owned=%zu ready=%zu retired=%zu/%zu/%zu) Default(owned=%zu ready=%zu retired=%zu/%zu/%zu)",
			UploadPool.GetStandardPageCount(), UploadPool.GetReadyPageCount(),
			UploadPool.GetRetiredPageCountForQueue(0), UploadPool.GetRetiredPageCountForQueue(1), UploadPool.GetRetiredPageCountForQueue(2),
			DefaultPool.GetStandardPageCount(), DefaultPool.GetReadyPageCount(),
			DefaultPool.GetRetiredPageCountForQueue(0), DefaultPool.GetRetiredPageCountForQueue(1), DefaultPool.GetRetiredPageCountForQueue(2));

		if (Adapter)
		{
			const auto& Ring = Adapter->GetTransientUploadRing();
			const double MB = 1024.0 * 1024.0;
			core::LOG(core::log_inf,
				L"[D3D12] TransientUploadRing size=%.1fMB used=%.1fMB head=%.1fMB tail=%.1fMB outstandingFences=%zu",
				(double)Ring.GetSizeBytes() / MB,
				(double)Ring.GetUsedBytes() / MB,
				(double)Ring.GetHeadBytes() / MB,
				(double)Ring.GetTailBytes() / MB,
				Ring.GetOutstandingFenceCount());
		}

		{
			static uint64_t sPrevWrap = 0, sPrevWrapBytes = 0;
			static uint64_t sPrevWait = 0, sPrevWaitBytes = 0;
			static uint64_t sPrevFail = 0;

			const uint64_t cWrap = D3D12CreateStats::TransientRing_WrapCount().load(std::memory_order_relaxed);
			const uint64_t cWrapBytes = D3D12CreateStats::TransientRing_WrapBytes().load(std::memory_order_relaxed);
			const uint64_t cWait = D3D12CreateStats::TransientRing_WaitCount().load(std::memory_order_relaxed);
			const uint64_t cWaitBytes = D3D12CreateStats::TransientRing_WaitBytes().load(std::memory_order_relaxed);
			const uint64_t cFail = D3D12CreateStats::TransientRing_AllocFailCount().load(std::memory_order_relaxed);

			const double MB = 1024.0 * 1024.0;
			core::LOG(core::log_inf,
				L"[D3D12] TransientRing/s wrap=%llu(%.1fMB) wait=%llu(%.1fMB) allocFail=%llu",
				(unsigned long long)(cWrap - sPrevWrap),
				(double)(cWrapBytes - sPrevWrapBytes) / MB,
				(unsigned long long)(cWait - sPrevWait),
				(double)(cWaitBytes - sPrevWaitBytes) / MB,
				(unsigned long long)(cFail - sPrevFail));

			sPrevWrap = cWrap; sPrevWrapBytes = cWrapBytes;
			sPrevWait = cWait; sPrevWaitBytes = cWaitBytes;
			sPrevFail = cFail;
		}

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

		// One-shot build stamp so we can verify the exact binary producing this log.
		{
			static bool sPrintedBuildStamp = false;
			if (!sPrintedBuildStamp)
			{
				sPrintedBuildStamp = true;
				core::LOG(core::log_inf,
					L"[D3D12] BuildStamp %S %S (WCCommitDelta=ON)",
					__DATE__, __TIME__);
			}
		}

		// Attribute gradual VMemPrivate WC commit growth to mapped regions.
		// This is the key signal when Create/Map counts are flat but WC bytes keeps increasing.
		D3D12UploadWCDiagnostics_DumpMappedRegionCommitDeltas();
		D3D12UploadWCDiagnostics_DumpProcessWideWcCommitDeltas();

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
			static uint64_t sPrevUploadLargeCnt = 0, sPrevUploadLargeBytes = 0;
			static uint64_t sPrevReuseReady = 0, sPrevFenceWait = 0;
			static uint64_t sPrevDiscardStd = 0, sPrevStdCacheEvict = 0, sPrevLargeDestroy = 0;

			const uint64_t curUploadCnt = D3D12CreateStats::LinearPage_CreateCount_Upload().load(std::memory_order_relaxed);
			const uint64_t curUploadBytes = D3D12CreateStats::LinearPage_CreateBytes_Upload().load(std::memory_order_relaxed);
			const uint64_t curDefaultCnt = D3D12CreateStats::LinearPage_CreateCount_Default().load(std::memory_order_relaxed);
			const uint64_t curDefaultBytes = D3D12CreateStats::LinearPage_CreateBytes_Default().load(std::memory_order_relaxed);
			const uint64_t curUploadLargeCnt = D3D12CreateStats::LinearPage_UploadLargeCreateCount().load(std::memory_order_relaxed);
			const uint64_t curUploadLargeBytes = D3D12CreateStats::LinearPage_UploadLargeCreateBytes().load(std::memory_order_relaxed);
			const uint64_t curReuseReady = D3D12CreateStats::LinearPage_ReuseFromReadyCount().load(std::memory_order_relaxed);
			const uint64_t curFenceWait = D3D12CreateStats::LinearPage_FenceWaitReuseCount().load(std::memory_order_relaxed);
			const uint64_t curDiscardStd = D3D12CreateStats::LinearPage_DiscardStandardPageCount().load(std::memory_order_relaxed);
			const uint64_t curStdCacheEvict = D3D12CreateStats::LinearPage_StandardCacheReleaseCount().load(std::memory_order_relaxed);
			const uint64_t curLargeDestroy = D3D12CreateStats::LinearPage_LargePageDestroyedCount().load(std::memory_order_relaxed);

			const uint64_t dUploadCnt = curUploadCnt - sPrevCpuPageUploadCnt;
			const uint64_t dUploadBytes = curUploadBytes - sPrevCpuPageUploadBytes;
			const uint64_t dDefaultCnt = curDefaultCnt - sPrevGpuPageDefaultCnt;
			const uint64_t dDefaultBytes = curDefaultBytes - sPrevGpuPageDefaultBytes;
			const uint64_t dUploadLargeCnt = curUploadLargeCnt - sPrevUploadLargeCnt;
			const uint64_t dUploadLargeBytes = curUploadLargeBytes - sPrevUploadLargeBytes;
			const uint64_t dReuseReady = curReuseReady - sPrevReuseReady;
			const uint64_t dFenceWait = curFenceWait - sPrevFenceWait;
			const uint64_t dDiscardStd = curDiscardStd - sPrevDiscardStd;
			const uint64_t dStdCacheEvict = curStdCacheEvict - sPrevStdCacheEvict;
			const uint64_t dLargeDestroy = curLargeDestroy - sPrevLargeDestroy;

			const uint64_t dUploadStdCnt = (dUploadCnt > dUploadLargeCnt) ? (dUploadCnt - dUploadLargeCnt) : 0;
			const uint64_t dUploadStdBytes = (dUploadBytes > dUploadLargeBytes) ? (dUploadBytes - dUploadLargeBytes) : 0;

			sPrevCpuPageUploadCnt = curUploadCnt;
			sPrevCpuPageUploadBytes = curUploadBytes;
			sPrevGpuPageDefaultCnt = curDefaultCnt;
			sPrevGpuPageDefaultBytes = curDefaultBytes;
			sPrevUploadLargeCnt = curUploadLargeCnt;
			sPrevUploadLargeBytes = curUploadLargeBytes;
			sPrevReuseReady = curReuseReady;
			sPrevFenceWait = curFenceWait;
			sPrevDiscardStd = curDiscardStd;
			sPrevStdCacheEvict = curStdCacheEvict;
			sPrevLargeDestroy = curLargeDestroy;

			core::LOG(core::log_inf,
				L"[D3D12] DirectCreates LinearPages Upload(+%llu %.1fMB) Default(+%llu %.1fMB)",
				(unsigned long long)dUploadCnt, (double)dUploadBytes / MB,
				(unsigned long long)dDefaultCnt, (double)dDefaultBytes / MB);

			core::LOG(core::log_inf,
				L"[D3D12] LinearPageActivity per1s Upload std(+%llu %.1fMB) large(+%llu %.1fMB) reuseReady(+%llu) fenceWaitToReuse(+%llu) discardStdPages(+%llu) stdCacheEvict(+%llu) largeDestroy(+%llu)",
				(unsigned long long)dUploadStdCnt, (double)dUploadStdBytes / MB,
				(unsigned long long)dUploadLargeCnt, (double)dUploadLargeBytes / MB,
				(unsigned long long)dReuseReady,
				(unsigned long long)dFenceWait,
				(unsigned long long)dDiscardStd,
				(unsigned long long)dStdCacheEvict,
				(unsigned long long)dLargeDestroy);
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

