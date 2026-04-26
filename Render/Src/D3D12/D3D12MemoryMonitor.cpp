#include "D3D12/D3D12MemoryMonitor.h"

#include "D3D12/D3D12Adapter.h"

#include "core/commandline.h"
#include "core/logger.h"

#include <windows.h>
#include <Psapi.h>
#include <vector>

#include <dxgi1_4.h>

namespace RenderCore
{
	bool D3D12MemoryMonitor::IsEnabled()
	{
		return core::CommandLine::Get().GetName("d3d12_memmon");
	}

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

	void D3D12MemoryMonitor::TickOncePerSecond(const std::shared_ptr<FD3D12Adapter>& Adapter, const std::shared_ptr<FD3D12Device>& Device)
	{
		if (!IsEnabled())
			return;
		if (!TickGateOncePerSecond())
			return;
		if (!Adapter || !Device)
			return;

		// DXGI video memory
		if (IDXGIAdapter* DxgiAdapter = Adapter->GetAdapter())
		{
			IDXGIAdapter3* Adapter3 = nullptr;
			if (SUCCEEDED(DxgiAdapter->QueryInterface(IID_PPV_ARGS(&Adapter3))) && Adapter3)
			{
				DXGI_QUERY_VIDEO_MEMORY_INFO Local = {};
				DXGI_QUERY_VIDEO_MEMORY_INFO NonLocal = {};
				Adapter3->QueryVideoMemoryInfo(0, DXGI_MEMORY_SEGMENT_GROUP_LOCAL, &Local);
				Adapter3->QueryVideoMemoryInfo(0, DXGI_MEMORY_SEGMENT_GROUP_NON_LOCAL, &NonLocal);

				const double MB = 1024.0 * 1024.0;
				core::LOG(core::log_inf,
					L"[D3D12] VidMem Local(Usage=%.1fMB Budget=%.1fMB Resv=%.1fMB Avail=%.1fMB) NonLocal(Usage=%.1fMB Budget=%.1fMB)",
					(double)Local.CurrentUsage / MB,
					(double)Local.Budget / MB,
					(double)Local.CurrentReservation / MB,
					(double)Local.AvailableForReservation / MB,
					(double)NonLocal.CurrentUsage / MB,
					(double)NonLocal.Budget / MB);

				Adapter3->Release();
			}
		}

		// Process memory
		{
			PROCESS_MEMORY_COUNTERS_EX pmc = {};
			pmc.cb = sizeof(pmc);
			if (::GetProcessMemoryInfo(::GetCurrentProcess(), (PROCESS_MEMORY_COUNTERS*)&pmc, sizeof(pmc)))
			{
				const double MB = 1024.0 * 1024.0;
				core::LOG(core::log_inf,
					L"[D3D12] ProcMem WorkingSet=%.1fMB Private=%.1fMB Pagefile=%.1fMB",
					(double)pmc.WorkingSetSize / MB,
					(double)pmc.PrivateUsage / MB,
					(double)pmc.PagefileUsage / MB);
			}
		}

		// Heap walk (CPU heap pressure)
		{
			DWORD heapCount = ::GetProcessHeaps(0, nullptr);
			if (heapCount > 0 && heapCount < 1024 * 1024)
			{
				std::vector<HANDLE> heaps;
				heaps.resize(heapCount);
				heapCount = ::GetProcessHeaps(heapCount, heaps.data());

				uint64_t allocatedBusy = 0;
				uint64_t busyBlocks = 0;
				for (DWORD i = 0; i < heapCount; ++i)
				{
					if (::HeapLock(heaps[i]))
					{
						PROCESS_HEAP_ENTRY entry = {};
						while (::HeapWalk(heaps[i], &entry))
						{
							if (entry.wFlags & PROCESS_HEAP_ENTRY_BUSY)
							{
								allocatedBusy += (uint64_t)entry.cbData;
								++busyBlocks;
							}
						}
						::HeapUnlock(heaps[i]);
					}
				}

				const double MB = 1024.0 * 1024.0;
				core::LOG(core::log_inf,
					L"[D3D12] Heaps Count=%u BusyBlocks=%llu BusyAllocated=%.1fMB",
					(unsigned)heapCount,
					(unsigned long long)busyBlocks,
					(double)allocatedBusy / MB);
			}
		}

		// Virtual memory breakdown (pinpoints whether growth is MEM_PRIVATE vs mapped/image).
		{
			uint64_t commitPrivate = 0, commitMapped = 0, commitImage = 0;
			uint64_t regions = 0;
			uint64_t largestRegion = 0;

			uint64_t privRW = 0, privRO = 0, privER = 0, privERW = 0, privNoAccess = 0, privOther = 0;
			uint64_t privWCBytes = 0;
			uint64_t privWCRegions = 0;
			uint64_t privWC_Bucket_Le1MB = 0, privWC_Bucket_Le4MB = 0, privWC_Bucket_Le16MB = 0, privWC_Bucket_Le32MB = 0, privWC_Bucket_Gt32MB = 0;

			struct TopRegion
			{
				void* Base = nullptr;
				uint64_t Size = 0;
				DWORD Protect = 0;
			};
			TopRegion top[5] = {};
			TopRegion topWC[5] = {};

			uint8_t* p = nullptr;
			MEMORY_BASIC_INFORMATION mbi = {};
			while (::VirtualQuery(p, &mbi, sizeof(mbi)) == sizeof(mbi))
			{
				++regions;
				if (mbi.State == MEM_COMMIT)
				{
					const uint64_t sz = (uint64_t)mbi.RegionSize;
					largestRegion = (std::max)(largestRegion, sz);
					switch (mbi.Type)
					{
					case MEM_PRIVATE: commitPrivate += sz; break;
					case MEM_MAPPED:  commitMapped += sz; break;
					case MEM_IMAGE:   commitImage += sz; break;
					default: break;
					}

					if (mbi.Type == MEM_PRIVATE)
					{
						TopRegion cand;
						cand.Base = mbi.BaseAddress;
						cand.Size = sz;
						cand.Protect = mbi.Protect;
						for (int ti = 0; ti < 5; ++ti)
						{
							if (cand.Size > top[ti].Size)
							{
								TopRegion tmp = top[ti];
								top[ti] = cand;
								cand = tmp;
							}
						}

						const DWORD prot = mbi.Protect & 0xFF;
						switch (prot)
						{
						case PAGE_READWRITE: privRW += (uint64_t)mbi.RegionSize; break;
						case PAGE_READONLY: privRO += (uint64_t)mbi.RegionSize; break;
						case PAGE_EXECUTE_READ: privER += (uint64_t)mbi.RegionSize; break;
						case PAGE_EXECUTE_READWRITE: privERW += (uint64_t)mbi.RegionSize; break;
						case PAGE_NOACCESS: privNoAccess += (uint64_t)mbi.RegionSize; break;
						default: privOther += (uint64_t)mbi.RegionSize; break;
						}

						if ((mbi.Protect & PAGE_WRITECOMBINE) != 0)
						{
							privWCBytes += (uint64_t)mbi.RegionSize;
							++privWCRegions;

							if (sz <= 1ull * 1024ull * 1024ull) privWC_Bucket_Le1MB += sz;
							else if (sz <= 4ull * 1024ull * 1024ull) privWC_Bucket_Le4MB += sz;
							else if (sz <= 16ull * 1024ull * 1024ull) privWC_Bucket_Le16MB += sz;
							else if (sz <= 32ull * 1024ull * 1024ull) privWC_Bucket_Le32MB += sz;
							else privWC_Bucket_Gt32MB += sz;

							TopRegion wcCand;
							wcCand.Base = mbi.BaseAddress;
							wcCand.Size = sz;
							wcCand.Protect = mbi.Protect;
							for (int ti = 0; ti < 5; ++ti)
							{
								if (wcCand.Size > topWC[ti].Size)
								{
									TopRegion tmp = topWC[ti];
									topWC[ti] = wcCand;
									wcCand = tmp;
								}
							}
						}
					}
				}

				uint8_t* next = (uint8_t*)mbi.BaseAddress + (size_t)mbi.RegionSize;
				if (next <= p)
					break;
				p = next;
			}

			const double MB = 1024.0 * 1024.0;
			core::LOG(core::log_inf,
				L"[D3D12] VMem Regions=%llu Commit Private=%.1fMB Mapped=%.1fMB Image=%.1fMB Largest=%.1fMB",
				(unsigned long long)regions,
				(double)commitPrivate / MB,
				(double)commitMapped / MB,
				(double)commitImage / MB,
				(double)largestRegion / MB);

			core::LOG(core::log_inf,
				L"[D3D12] VMemPrivate Protect RW=%.1fMB RO=%.1fMB ER=%.1fMB ERW=%.1fMB NoAccess=%.1fMB Other=%.1fMB",
				(double)privRW / MB,
				(double)privRO / MB,
				(double)privER / MB,
				(double)privERW / MB,
				(double)privNoAccess / MB,
				(double)privOther / MB);

			static uint64_t sPrevPrivWCBytes = 0, sPrevPrivWCRegions = 0;
			uint64_t dWCBytes = 0;
			uint64_t dWCRegions = 0;
			if (privWCBytes >= sPrevPrivWCBytes) dWCBytes = privWCBytes - sPrevPrivWCBytes;
			if (privWCRegions >= sPrevPrivWCRegions) dWCRegions = privWCRegions - sPrevPrivWCRegions;
			sPrevPrivWCBytes = privWCBytes;
			sPrevPrivWCRegions = privWCRegions;

			core::LOG(core::log_inf,
				L"[D3D12] VMemPrivate WC Regions=%llu (+%llu) Bytes=%.1fMB (+%.1fMB) Buckets(<=1=%.1f <=4=%.1f <=16=%.1f <=32=%.1f >32=%.1f)",
				(unsigned long long)privWCRegions,
				(unsigned long long)dWCRegions,
				(double)privWCBytes / MB,
				(double)dWCBytes / MB,
				(double)privWC_Bucket_Le1MB / MB,
				(double)privWC_Bucket_Le4MB / MB,
				(double)privWC_Bucket_Le16MB / MB,
				(double)privWC_Bucket_Le32MB / MB,
				(double)privWC_Bucket_Gt32MB / MB);

			if (dWCBytes > 0)
			{
				void* frames[16] = {};
				const USHORT n = ::RtlCaptureStackBackTrace(0, 16, frames, nullptr);
				if (n > 0)
				{
					core::LOG(core::log_inf,
						L"[D3D12] VMemPrivate WC +%.1fMB observed here. Stack: %p %p %p %p %p %p %p %p",
						(double)dWCBytes / MB,
						frames[0], (n > 1 ? frames[1] : nullptr), (n > 2 ? frames[2] : nullptr), (n > 3 ? frames[3] : nullptr),
						(n > 4 ? frames[4] : nullptr), (n > 5 ? frames[5] : nullptr), (n > 6 ? frames[6] : nullptr), (n > 7 ? frames[7] : nullptr));
				}
			}

			core::LOG(core::log_inf,
				L"[D3D12] VMemPrivate WC Top Base=%p Size=%.1fMB Prot=0x%X | %p %.1fMB 0x%X | %p %.1fMB 0x%X | %p %.1fMB 0x%X | %p %.1fMB 0x%X",
				topWC[0].Base, (double)topWC[0].Size / MB, (unsigned)topWC[0].Protect,
				topWC[1].Base, (double)topWC[1].Size / MB, (unsigned)topWC[1].Protect,
				topWC[2].Base, (double)topWC[2].Size / MB, (unsigned)topWC[2].Protect,
				topWC[3].Base, (double)topWC[3].Size / MB, (unsigned)topWC[3].Protect,
				topWC[4].Base, (double)topWC[4].Size / MB, (unsigned)topWC[4].Protect);

			core::LOG(core::log_inf,
				L"[D3D12] VMemPrivate Top Base=%p Size=%.1fMB Prot=0x%X | %p %.1fMB 0x%X | %p %.1fMB 0x%X | %p %.1fMB 0x%X | %p %.1fMB 0x%X",
				top[0].Base, (double)top[0].Size / MB, (unsigned)top[0].Protect,
				top[1].Base, (double)top[1].Size / MB, (unsigned)top[1].Protect,
				top[2].Base, (double)top[2].Size / MB, (unsigned)top[2].Protect,
				top[3].Base, (double)top[3].Size / MB, (unsigned)top[3].Protect,
				top[4].Base, (double)top[4].Size / MB, (unsigned)top[4].Protect);
		}
	}
}

