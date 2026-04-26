#include "D3D12/D3D12MemoryMonitor.h"

#include "D3D12/D3D12Adapter.h"
#include "D3D12/D3D12CallStats.h"

#include "core/commandline.h"
#include "core/logger.h"

#include <windows.h>
#include <DbgHelp.h>
#include <Psapi.h>
#include <vector>
#include <mutex>
#include <unordered_set>

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

	static void EnsureDbgHelpInitialized()
	{
		static std::once_flag sOnce;
		std::call_once(sOnce, []() {
			::SymSetOptions(SYMOPT_UNDNAME | SYMOPT_DEFERRED_LOADS | SYMOPT_LOAD_LINES);
			// FALSE: do not invade all modules (TRUE loads symbol tables for every DLL and can grow Private MB/s while memmon runs).
			HANDLE proc = ::GetCurrentProcess();
			::SymInitialize(proc, nullptr, FALSE);

			// Even with invade=FALSE, make sure the main module's PDB is loaded so SymFromAddr can resolve our own code.
			// This keeps overhead low while making stacks actionable.
			wchar_t modulePathW[MAX_PATH] = {};
			if (::GetModuleFileNameW(nullptr, modulePathW, (DWORD)_countof(modulePathW)) > 0)
			{
				HMODULE hMod = ::GetModuleHandleW(nullptr);
				::SymLoadModuleExW(proc, nullptr, modulePathW, nullptr, (DWORD64)(uintptr_t)hMod, 0, nullptr, 0);
			}
		});
	}

	static void FormatAddrSymbol(wchar_t* out, size_t outChars, void* addr)
	{
		if (!out || outChars == 0)
			return;
		out[0] = 0;
		if (!addr)
		{
			_snwprintf_s(out, outChars, _TRUNCATE, L"(null)");
			return;
		}

		EnsureDbgHelpInitialized();

		const DWORD64 a = (DWORD64)(uintptr_t)addr;

		char symBuffer[sizeof(SYMBOL_INFO) + MAX_SYM_NAME] = {};
		SYMBOL_INFO* sym = (SYMBOL_INFO*)symBuffer;
		sym->SizeOfStruct = sizeof(SYMBOL_INFO);
		sym->MaxNameLen = MAX_SYM_NAME;

		DWORD64 disp = 0;
		IMAGEHLP_LINE64 line = {};
		line.SizeOfStruct = sizeof(line);
		DWORD lineDisp = 0;

		wchar_t nameW[256] = {};
		const bool hasSym = ::SymFromAddr(::GetCurrentProcess(), a, &disp, sym) != 0;
		if (hasSym)
			MultiByteToWideChar(CP_UTF8, 0, sym->Name, -1, nameW, (int)_countof(nameW));

		const bool hasLine = ::SymGetLineFromAddr64(::GetCurrentProcess(), a, &lineDisp, &line) != 0;

		if (hasSym && hasLine)
		{
			wchar_t fileW[MAX_PATH] = {};
			MultiByteToWideChar(CP_UTF8, 0, line.FileName, -1, fileW, (int)_countof(fileW));
			_snwprintf_s(out, outChars, _TRUNCATE, L"%s +0x%llX (%s:%lu)", nameW, (unsigned long long)disp, fileW, (unsigned long)line.LineNumber);
		}
		else if (hasSym)
		{
			_snwprintf_s(out, outChars, _TRUNCATE, L"%s +0x%llX", nameW, (unsigned long long)disp);
		}
		else
		{
			_snwprintf_s(out, outChars, _TRUNCATE, L"%p", addr);
		}
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

			// Correlate WC deltas with high-frequency engine D3D12 call activity (per 1s tick).
			{
				const Render::D3D12CallStats::Snapshot cs = Render::D3D12CallStats::SnapshotAndReset();
				core::LOG(core::log_inf,
					L"[D3D12] CallStats(1s) Exec=%llu (lists=%llu) Signal=%llu Present=%llu CreateCommitted=%llu Map=%llu Unmap=%llu FenceSetEvent=%llu Wait=%llu | DirectFence Immediate=%llu Deferred=%llu | Barriers Add=%llu FlushCalls=%llu Flushed=%llu | Copy=%.1fMB Upload=%.1fMB",
					(unsigned long long)cs.ExecuteCommandListsCalls,
					(unsigned long long)cs.ExecuteCommandListsLists,
					(unsigned long long)cs.QueueSignalCalls,
					(unsigned long long)cs.SwapchainPresentCalls,
					(unsigned long long)cs.CreateCommittedResourceCalls,
					(unsigned long long)cs.ResourceMapCalls,
					(unsigned long long)cs.ResourceUnmapCalls,
					(unsigned long long)cs.FenceSetEventOnCompletionCalls,
					(unsigned long long)cs.WaitForSingleObjectCalls,
					(unsigned long long)cs.DirectFenceImmediateSignalCalls,
					(unsigned long long)cs.DirectFenceDeferredReserveCalls,
					(unsigned long long)cs.ResourceBarrierAdds,
					(unsigned long long)cs.ResourceBarrierFlushCalls,
					(unsigned long long)cs.ResourceBarrierFlushed,
					(double)cs.CopyBytes / MB,
					(double)cs.UploadBytes / MB);
			}

			// For newly observed large WC regions, print attribution (MEM_PRIVATE vs MEM_MAPPED, etc.).
			// This helps distinguish upload-heap like allocations from mapped sections.
			{
				static std::mutex sWcMu;
				static std::unordered_set<void*> sSeenWcAllocBases;

				auto TypeToStr = [](DWORD type) -> const wchar_t*
				{
					switch (type)
					{
					case MEM_PRIVATE: return L"PRIVATE";
					case MEM_MAPPED:  return L"MAPPED";
					case MEM_IMAGE:   return L"IMAGE";
					default:          return L"UNKNOWN";
					}
				};

				for (int i = 0; i < 5; ++i)
				{
					TopRegion& R = topWC[i];
					if (!R.Base || R.Size < 16ull * 1024ull * 1024ull)
						continue;

					MEMORY_BASIC_INFORMATION mbi2 = {};
					if (::VirtualQuery(R.Base, &mbi2, sizeof(mbi2)) != sizeof(mbi2))
						continue;

					void* allocBase = mbi2.AllocationBase ? mbi2.AllocationBase : mbi2.BaseAddress;
					bool bNew = false;
					{
						std::lock_guard<std::mutex> lock(sWcMu);
						bNew = sSeenWcAllocBases.insert(allocBase).second;
						// Avoid unbounded growth; never clear() the whole set — that re-fires "WCRegion New" for old bases
						// and amplifies logging / DbgHelp work every tick until the set refills.
						while (sSeenWcAllocBases.size() > 512)
						{
							auto it = sSeenWcAllocBases.begin();
							if (it == sSeenWcAllocBases.end())
								break;
							sSeenWcAllocBases.erase(it);
						}
					}
					if (!bNew)
						continue;

					wchar_t mappedName[MAX_PATH] = {};
					mappedName[0] = 0;
					if (mbi2.Type == MEM_MAPPED)
					{
						// Best-effort: identify mapped section source.
						::GetMappedFileNameW(::GetCurrentProcess(), allocBase, mappedName, (DWORD)_countof(mappedName));
					}

					core::LOG(core::log_inf,
						L"[D3D12] WCRegion New Base=%p Size=%.1fMB Prot=0x%X Type=%s State=0x%X AllocBase=%p Mapped=%s",
						mbi2.BaseAddress,
						(double)(uint64_t)mbi2.RegionSize / MB,
						(unsigned)mbi2.Protect,
						TypeToStr(mbi2.Type),
						(unsigned)mbi2.State,
						allocBase,
						(mappedName[0] ? mappedName : L"-"));
				}
			}

			if (dWCBytes > 0)
			{
				// RtlCaptureStackBackTrace + SymFromAddr pulls DbgHelp and can allocate MB/s of private heap
				// while this monitor runs (looks like a leak). Keep stacks optional and heavily throttled.
				if (!core::CommandLine::Get().GetName("d3d12_memmon_stacks"))
				{
					core::LOG(core::log_inf,
						L"[D3D12] VMemPrivate WC +%.1fMB (stacks disabled; use d3d12_memmon_stacks=1 for symbolized backtraces)",
						(double)dWCBytes / MB);
				}
				else
				{
					static thread_local bool sReentryGuard = false;
					if (sReentryGuard)
						goto AfterWCStack;
					sReentryGuard = true;

					static ULONGLONG sLastSymMs = 0;
					const ULONGLONG nowMs = ::GetTickCount64();
					// Even with stacks=1, do not symbolize on every 1s tick — DbgHelp can dominate private growth.
					static constexpr ULONGLONG kSymMinIntervalMs = 15000;
					const bool bDoSymbols = (nowMs - sLastSymMs >= kSymMinIntervalMs);
					if (bDoSymbols)
						sLastSymMs = nowMs;

					void* frames[16] = {};
					const USHORT n = ::RtlCaptureStackBackTrace(0, 16, frames, nullptr);
					if (n > 0)
					{
						if (bDoSymbols)
						{
							wchar_t s0[256] = {}, s1[256] = {}, s2[256] = {}, s3[256] = {};
							wchar_t s4[256] = {}, s5[256] = {}, s6[256] = {}, s7[256] = {};
							FormatAddrSymbol(s0, _countof(s0), frames[0]);
							FormatAddrSymbol(s1, _countof(s1), (n > 1 ? frames[1] : nullptr));
							FormatAddrSymbol(s2, _countof(s2), (n > 2 ? frames[2] : nullptr));
							FormatAddrSymbol(s3, _countof(s3), (n > 3 ? frames[3] : nullptr));
							FormatAddrSymbol(s4, _countof(s4), (n > 4 ? frames[4] : nullptr));
							FormatAddrSymbol(s5, _countof(s5), (n > 5 ? frames[5] : nullptr));
							FormatAddrSymbol(s6, _countof(s6), (n > 6 ? frames[6] : nullptr));
							FormatAddrSymbol(s7, _countof(s7), (n > 7 ? frames[7] : nullptr));

							core::LOG(core::log_inf,
								L"[D3D12] VMemPrivate WC +%.1fMB observed here. Stack: %s | %s | %s | %s | %s | %s | %s | %s",
								(double)dWCBytes / MB,
								s0, s1, s2, s3, s4, s5, s6, s7);
						}
						else
						{
							core::LOG(core::log_inf,
								L"[D3D12] VMemPrivate WC +%.1fMB (raw PCs; DbgHelp symbolize throttled to every %llu s — turn off d3d12_memmon_stacks when measuring leaks)",
								(double)dWCBytes / MB,
								(unsigned long long)(kSymMinIntervalMs / 1000));
							core::LOG(core::log_inf,
								L"[D3D12] VMemPrivate WC raw stack: %p | %p | %p | %p | %p | %p | %p | %p",
								frames[0],
								(n > 1 ? frames[1] : nullptr),
								(n > 2 ? frames[2] : nullptr),
								(n > 3 ? frames[3] : nullptr),
								(n > 4 ? frames[4] : nullptr),
								(n > 5 ? frames[5] : nullptr),
								(n > 6 ? frames[6] : nullptr),
								(n > 7 ? frames[7] : nullptr));
						}
					}

					sReentryGuard = false;
				}
			}
		AfterWCStack:

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

