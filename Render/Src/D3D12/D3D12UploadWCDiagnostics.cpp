#include "D3D12/D3D12UploadWCDiagnostics.h"
#include "win/win32.h"
#include "RHI/RHI.h"
#include "core/commandline.h"
#include "core/logger.h"
#include <DbgHelp.h>
#include <unordered_set>

namespace RenderCore
{
	namespace
	{
		struct FMappedWcRegion
		{
			void* Base = nullptr;        // AllocationBase
			uint64_t SizeBytes = 0;      // best-effort tracked size (may be RegionSize)
			wchar_t Tag[64] = {};
			uint64_t LastCommittedBytes = 0;
		};

		static uint64_t QueryCommittedWcBytes(void* Base, uint64_t SizeBytes)
		{
			if (!Base || SizeBytes == 0)
				return 0;

			uint8_t* p = (uint8_t*)Base;
			uint8_t* end = p + SizeBytes;
			uint64_t committed = 0;
			while (p < end)
			{
				MEMORY_BASIC_INFORMATION mbi = {};
				if (::VirtualQuery(p, &mbi, sizeof(mbi)) != sizeof(mbi))
					break;

				const uint8_t* regionBase = (const uint8_t*)mbi.BaseAddress;
				uint8_t* regionEnd = (uint8_t*)mbi.BaseAddress + (size_t)mbi.RegionSize;
				if (regionEnd > end)
					regionEnd = end;

				const bool bCommitted = (mbi.State == MEM_COMMIT);
				// Some drivers/runtime paths report WC on AllocationProtect rather than Protect for sub-regions.
				// Use either to classify a region as WC for attribution purposes.
				const bool bWC = ((mbi.Protect & PAGE_WRITECOMBINE) != 0) || ((mbi.AllocationProtect & PAGE_WRITECOMBINE) != 0);
				if (bCommitted && bWC)
					committed += (uint64_t)(regionEnd - (uint8_t*)mbi.BaseAddress);

				p = (uint8_t*)mbi.BaseAddress + (size_t)mbi.RegionSize;
			}
			return committed;
		}

		// Sum VirtualQuery region sizes from AllocationBase until we leave this allocation.
		static uint64_t DiscoverAllocationSpanBytes(void* AnyPtrInAllocation)
		{
			MEMORY_BASIC_INFORMATION first = {};
			if (::VirtualQuery(AnyPtrInAllocation, &first, sizeof(first)) != sizeof(first))
				return 0;
			void* const allocBase = first.AllocationBase;
			uint64_t span = 0;
			uint8_t* p = (uint8_t*)allocBase;
			for (;;)
			{
				MEMORY_BASIC_INFORMATION mbi = {};
				if (::VirtualQuery(p, &mbi, sizeof(mbi)) != sizeof(mbi))
					break;
				if (mbi.AllocationBase != allocBase)
					break;
				span += (uint64_t)mbi.RegionSize;
				uint8_t* next = (uint8_t*)mbi.BaseAddress + (size_t)mbi.RegionSize;
				if (next <= p)
					break;
				p = next;
			}
			return span;
		}

		static void FormatAddrSymbol(wchar_t* out, size_t outChars, void* addr);

		static std::mutex& RegionsMutex()
		{
			static std::mutex sMu;
			return sMu;
		}

		static std::vector<FMappedWcRegion>& Regions()
		{
			static std::vector<FMappedWcRegion> sRegions;
			return sRegions;
		}

		static void EnsureDbgHelpInitialized()
		{
			static std::once_flag sOnce;
			std::call_once(sOnce, []() {
				::SymSetOptions(SYMOPT_UNDNAME | SYMOPT_DEFERRED_LOADS | SYMOPT_LOAD_LINES);
				HANDLE proc = ::GetCurrentProcess();
				// FALSE: avoid SymInitialize(invade=TRUE) which loads symbols for every DLL and can grow Private bytes
				// for the whole memmon session (same rationale as D3D12MemoryMonitor).
				::SymInitialize(proc, nullptr, FALSE);

				// Add common local search paths so SymFromAddr can find PDBs next to the exe and in the current working directory.
				{
					wchar_t exePathW[MAX_PATH] = {};
					wchar_t cwdW[MAX_PATH] = {};
					::GetCurrentDirectoryW((DWORD)_countof(cwdW), cwdW);

					if (::GetModuleFileNameW(nullptr, exePathW, (DWORD)_countof(exePathW)) > 0)
					{
						// Strip filename to get the exe directory.
						wchar_t* lastSlash = wcsrchr(exePathW, L'\\');
						if (lastSlash)
							*lastSlash = 0;

						// "cwd;exeDir"
						wchar_t searchPathW[2 * MAX_PATH + 4] = {};
						_snwprintf_s(searchPathW, _countof(searchPathW), _TRUNCATE, L"%s;%s", cwdW, exePathW);
						::SymSetSearchPathW(proc, searchPathW);
					}
				}

				// Load the main module's symbols explicitly.
				wchar_t modulePathW[MAX_PATH] = {};
				if (::GetModuleFileNameW(nullptr, modulePathW, (DWORD)_countof(modulePathW)) > 0)
				{
					HMODULE hMod = ::GetModuleHandleW(nullptr);
					::SymLoadModuleExW(proc, nullptr, modulePathW, nullptr, (DWORD64)(uintptr_t)hMod, 0, nullptr, 0);
				}
			});
		}

		static std::mutex sProcessWideUnknownStackMu;
		static std::unordered_set<void*> sProcessWideUnknownStackLoggedBases;

		// When process-wide WC commit grows on an AllocationBase we never registered via OnUploadMap,
		// capture a one-time backtrace from the memmon sampling thread (may not be the code that touched pages).
		static void MaybeLogProcessWideUnknownWcStack(void* allocBase, uint64_t deltaBytes, uint64_t spanBytes, const wchar_t* tagW)
		{
			if (!allocBase || !tagW || tagW[0] != L'?' || tagW[1] != 0)
				return;
			// Stacks + DbgHelp are expensive and inflate Private/heap; keep them opt-in via memmon_stacks only.
			// memmon_deep alone enables HeapWalk/full VMem in D3D12MemoryMonitor, not sampler backtraces here.
			if (!D3D12RHI_ShouldEnableMemMonStacks())
				return;

			{
				std::lock_guard<std::mutex> lk(sProcessWideUnknownStackMu);
				if (!sProcessWideUnknownStackLoggedBases.insert(allocBase).second)
					return;
				if (sProcessWideUnknownStackLoggedBases.size() > 4096)
					sProcessWideUnknownStackLoggedBases.clear();
			}

			static thread_local bool sReentry = false;
			if (sReentry)
				return;
			sReentry = true;

			void* frames[16] = {};
			const USHORT n = ::RtlCaptureStackBackTrace(2, 16, frames, nullptr);

			wchar_t s0[256] = {}, s1[256] = {}, s2[256] = {}, s3[256] = {};
			wchar_t s4[256] = {}, s5[256] = {}, s6[256] = {}, s7[256] = {};
			if (n > 0)
			{
				FormatAddrSymbol(s0, _countof(s0), frames[0]);
				FormatAddrSymbol(s1, _countof(s1), (n > 1 ? frames[1] : nullptr));
				FormatAddrSymbol(s2, _countof(s2), (n > 2 ? frames[2] : nullptr));
				FormatAddrSymbol(s3, _countof(s3), (n > 3 ? frames[3] : nullptr));
				FormatAddrSymbol(s4, _countof(s4), (n > 4 ? frames[4] : nullptr));
				FormatAddrSymbol(s5, _countof(s5), (n > 5 ? frames[5] : nullptr));
				FormatAddrSymbol(s6, _countof(s6), (n > 6 ? frames[6] : nullptr));
				FormatAddrSymbol(s7, _countof(s7), (n > 7 ? frames[7] : nullptr));
			}

			const double MB = 1024.0 * 1024.0;
			core::LOG(core::log_inf,
				L"[D3D12] WCCommitUnknown stack allocBase=%p +%.1fMB span=%.1fMB (sampler; commit may be async) : %s | %s | %s | %s | %s | %s | %s | %s",
				allocBase,
				(double)deltaBytes / MB,
				(double)spanBytes / MB,
				s0, s1, s2, s3, s4, s5, s6, s7);

			sReentry = false;
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
				// Fall back to "module+offset" when PDBs aren't available/resolvable.
				HMODULE hMod = nullptr;
				if (::GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
						(LPCWSTR)addr, &hMod) && hMod)
				{
					wchar_t modPathW[MAX_PATH] = {};
					if (::GetModuleFileNameW(hMod, modPathW, (DWORD)_countof(modPathW)) > 0)
					{
						const uint64_t base = (uint64_t)(uintptr_t)hMod;
						const uint64_t off = (uint64_t)(uintptr_t)addr - base;
						// Keep only filename for readability.
						const wchar_t* modNameW = wcsrchr(modPathW, L'\\');
						modNameW = modNameW ? (modNameW + 1) : modPathW;
						_snwprintf_s(out, outChars, _TRUNCATE, L"%s+0x%llX", modNameW, (unsigned long long)off);
						return;
					}
				}

				_snwprintf_s(out, outChars, _TRUNCATE, L"%p", addr);
			}
		}
	}

	static int TagSpecificityRank(const wchar_t* t)
	{
		if (!t || !t[0])
			return 0;
		if (wcsstr(t, L"LinearPage_BuddyAllocator") != nullptr || wcsstr(t, L"BuddyAllocatorHeap") != nullptr
			|| wcsstr(t, L"LinearPage_PlacedBuddy") != nullptr || wcsstr(t, L"PlacedBuddy") != nullptr)
			return 4;
		if (wcsstr(t, L"FastConstantAllocator") != nullptr)
			return 4;
		if (wcsstr(t, L"ImGui.") != nullptr)
			return 4;
		if (wcsstr(t, L"FD3D12FastAllocatorPage") != nullptr)
			return 3;
		if (wcsstr(t, L"FD3D12Resource|") != nullptr)
			return 3;
		if (wcsstr(t, L"FD3D12Resource.Map") != nullptr)
			return 1;
		return 2;
	}

	void D3D12UploadWCDiagnostics_RegisterMappedRegion(const wchar_t* Tag, void* BasePtr, uint64_t SizeBytes)
	{
		if (!BasePtr || SizeBytes == 0)
			return;
		if (!RenderCore::D3D12RHI_ShouldEnableMemMon())
			return;

		MEMORY_BASIC_INFORMATION mbi = {};
		if (::VirtualQuery(BasePtr, &mbi, sizeof(mbi)) != sizeof(mbi))
			return;

		void* const allocBase = mbi.AllocationBase;
		const uint64_t spanBytes = DiscoverAllocationSpanBytes(BasePtr);
		const uint64_t offsetInAlloc = (uint64_t)((uint8_t*)BasePtr - (uint8_t*)allocBase);
		// Cover the whole D3D12 allocation (often starts with reserve/no-access at AllocationBase).
		uint64_t trackedSize = spanBytes;
		if (SizeBytes)
		{
			const uint64_t need = offsetInAlloc + SizeBytes;
			trackedSize = (need > trackedSize) ? need : trackedSize;
		}

		std::lock_guard<std::mutex> lock(RegionsMutex());
		auto& R = Regions();
		for (auto& e : R)
		{
			if (e.Base == allocBase)
			{
				// Best-effort: keep the max size we've seen.
				if (trackedSize > e.SizeBytes)
					e.SizeBytes = trackedSize;
				// Prefer a more specific tag when the same AllocationBase is registered from multiple paths
				// (e.g. FD3D12Resource::Map first, then LinearPage_BuddyAllocator / FastAllocatorPage).
				if (Tag && Tag[0] && TagSpecificityRank(Tag) > TagSpecificityRank(e.Tag))
					wcsncpy_s(e.Tag, Tag, _TRUNCATE);
				return;
			}
		}

		FMappedWcRegion NewE;
		NewE.Base = allocBase;
		NewE.SizeBytes = trackedSize;
		if (Tag && Tag[0])
			wcsncpy_s(NewE.Tag, Tag, _TRUNCATE);
		else
			wcsncpy_s(NewE.Tag, L"WCRegion", _TRUNCATE);
		NewE.LastCommittedBytes = QueryCommittedWcBytes(NewE.Base, NewE.SizeBytes);
		R.push_back(NewE);
		if (R.size() > 1024)
			R.erase(R.begin(), R.begin() + (R.size() - 1024));
	}

	void D3D12UploadWCDiagnostics_DumpMappedRegionCommitDeltas()
	{
		if (!RenderCore::D3D12RHI_ShouldEnableMemMon())
			return;

		// We only want to attribute gradual commit; skip if there are no tracked regions.
		std::lock_guard<std::mutex> lock(RegionsMutex());
		auto& R = Regions();
		if (R.empty())
			return;

		struct FDelta
		{
			const FMappedWcRegion* Region = nullptr;
			uint64_t DeltaBytes = 0;
			uint64_t CurCommitted = 0;
		};

		std::vector<FDelta> deltas;
		deltas.reserve(R.size());

		for (auto& e : R)
		{
			const uint64_t cur = QueryCommittedWcBytes(e.Base, e.SizeBytes);
			const uint64_t prev = e.LastCommittedBytes;
			const uint64_t d = (cur > prev) ? (cur - prev) : 0;
			e.LastCommittedBytes = cur;
			if (d > 0)
				deltas.push_back(FDelta{ &e, d, cur });
		}

		if (deltas.empty())
			return;

		std::sort(deltas.begin(), deltas.end(), [](const FDelta& a, const FDelta& b) { return a.DeltaBytes > b.DeltaBytes; });

		const double MB = 1024.0 * 1024.0;
		const size_t kMaxLines = 6;
		size_t printed = 0;
		// Log meaningful deltas (256KB+) so we don't miss ~26MB/s growth split across regions.
		const uint64_t kMinDeltaBytes = 256ull * 1024ull;
		for (const auto& d : deltas)
		{
			if (d.DeltaBytes < kMinDeltaBytes)
				break;
			core::LOG(core::log_inf,
				L"[D3D12] WCCommit/s +%.1fMB (cur=%.1fMB) tag=%s base=%p tracked=%.1fMB",
				(double)d.DeltaBytes / MB,
				(double)d.CurCommitted / MB,
				(d.Region ? d.Region->Tag : L"?"),
				(d.Region ? d.Region->Base : nullptr),
				(double)(d.Region ? d.Region->SizeBytes : 0) / MB);
			if (++printed >= kMaxLines)
				break;
		}
	}

	void D3D12UploadWCDiagnostics_DumpProcessWideWcCommitDeltas()
	{
		if (!RenderCore::D3D12RHI_ShouldEnableMemMon())
			return;

		// Aggregate committed MEM_PRIVATE WC bytes by AllocationBase over the entire VA space.
		// This matches what D3D12MemoryMonitor reports for VMemPrivate WC, but lets us attribute deltas.
		static std::unordered_map<void*, uint64_t> sPrevByBase;
		std::unordered_map<void*, uint64_t> curByBase;

		uint8_t* p = nullptr;
		MEMORY_BASIC_INFORMATION mbi = {};
		while (::VirtualQuery(p, &mbi, sizeof(mbi)) == sizeof(mbi))
		{
			if (mbi.State == MEM_COMMIT && mbi.Type == MEM_PRIVATE)
			{
				const bool bWC = ((mbi.Protect & PAGE_WRITECOMBINE) != 0) || ((mbi.AllocationProtect & PAGE_WRITECOMBINE) != 0);
				if (bWC)
				{
					curByBase[mbi.AllocationBase] += (uint64_t)mbi.RegionSize;
				}
			}

			uint8_t* next = (uint8_t*)mbi.BaseAddress + (size_t)mbi.RegionSize;
			if (next <= p)
				break;
			p = next;
		}

		struct FDelta
		{
			void* Base = nullptr;
			uint64_t DeltaBytes = 0;
			uint64_t CurBytes = 0;
		};

		std::vector<FDelta> deltas;
		deltas.reserve(curByBase.size());
		for (const auto& kv : curByBase)
		{
			const uint64_t prev = (sPrevByBase.count(kv.first) != 0) ? sPrevByBase[kv.first] : 0;
			const uint64_t cur = kv.second;
			if (cur > prev)
				deltas.push_back(FDelta{ kv.first, cur - prev, cur });
		}
		sPrevByBase.swap(curByBase);

		if (deltas.empty())
			return;

		std::sort(deltas.begin(), deltas.end(), [](const FDelta& a, const FDelta& b) { return a.DeltaBytes > b.DeltaBytes; });

		const double MB = 1024.0 * 1024.0;
		const uint64_t kMinDeltaBytes = 512ull * 1024ull; // 512KB+
		const size_t kMaxLines = 8;
		size_t printed = 0;
		for (const auto& d : deltas)
		{
			if (d.DeltaBytes < kMinDeltaBytes)
				break;

			// Try to attribute to a known mapped WC region tag (if we saw an UploadMap WC for it).
			const wchar_t* tagW = L"?";
			{
				std::lock_guard<std::mutex> lock(RegionsMutex());
				for (const auto& r : Regions())
				{
					if (r.Base == d.Base)
					{
						tagW = r.Tag;
						break;
					}
				}
			}

			// Also print the total span size of this allocation (often 32MB in the growing case).
			const uint64_t spanBytes = DiscoverAllocationSpanBytes(d.Base);
			core::LOG(core::log_inf,
				L"[D3D12] WCCommitAllocs/s +%.1fMB (cur=%.1fMB) allocBase=%p span=%.1fMB tag=%s",
				(double)d.DeltaBytes / MB,
				(double)d.CurBytes / MB,
				d.Base,
				(double)spanBytes / MB,
				tagW);

			MaybeLogProcessWideUnknownWcStack(d.Base, d.DeltaBytes, spanBytes, tagW);

			if (++printed >= kMaxLines)
				break;
		}
	}

	void D3D12UploadWCDiagnostics_OnUploadMap(const wchar_t* Tag, void* MappedPtr, uint64_t SizeBytes)
	{
		if (!MappedPtr || SizeBytes == 0)
			return;
		if (!RenderCore::D3D12RHI_ShouldEnableMemMon())
			return;

		MEMORY_BASIC_INFORMATION mbi = {};
		if (::VirtualQuery(MappedPtr, &mbi, sizeof(mbi)) != sizeof(mbi))
			return;

		// Always register the mapped span for process-wide WC commit deltas. Some drivers/runtime paths
		// only flip sub-ranges to PAGE_WRITECOMBINE after first touch; gating registration on "WC here"
		// would leave AllocationBase untagged and show up as tag=? in DumpProcessWideWcCommitDeltas.
		D3D12UploadWCDiagnostics_RegisterMappedRegion(Tag, MappedPtr, SizeBytes);

		const bool bWcHere = ((mbi.Protect & PAGE_WRITECOMBINE) != 0) || ((mbi.AllocationProtect & PAGE_WRITECOMBINE) != 0);
		if (!bWcHere)
			return;

		static std::mutex sMu;
		static std::unordered_set<void*> sSeenBases;
		{
			std::lock_guard<std::mutex> lock(sMu);
			if (!sSeenBases.insert(mbi.AllocationBase).second)
				return;
			if (sSeenBases.size() > 512)
				sSeenBases.clear();
		}

		static thread_local bool sReentryGuard = false;
		if (sReentryGuard)
			return;
		sReentryGuard = true;

		const double MB = 1024.0 * 1024.0;
		// Backtraces + DbgHelp: opt-in only (d3d12_memmon_stacks=1); memmon_deep does not imply stacks.
		const bool bWantStacks = RenderCore::D3D12RHI_ShouldEnableMemMonStacks();
		if (!bWantStacks)
		{
			core::LOG(core::log_inf,
				L"[D3D12] UploadMap WC (%s) ptr=%p size=%.1fMB allocBase=%p region=%.1fMB prot=0x%X (stacks off; add d3d12_memmon_stacks=1 for backtraces)",
				(Tag ? Tag : L"?"),
				MappedPtr,
				(double)SizeBytes / MB,
				mbi.AllocationBase,
				(double)(uint64_t)mbi.RegionSize / MB,
				(unsigned)mbi.Protect);
			sReentryGuard = false;
			return;
		}

		void* frames[16] = {};
		const USHORT n = ::RtlCaptureStackBackTrace(0, 16, frames, nullptr);

		wchar_t s0[256] = {}, s1[256] = {}, s2[256] = {}, s3[256] = {};
		wchar_t s4[256] = {}, s5[256] = {}, s6[256] = {}, s7[256] = {};
		if (n > 0)
		{
			FormatAddrSymbol(s0, _countof(s0), frames[0]);
			FormatAddrSymbol(s1, _countof(s1), (n > 1 ? frames[1] : nullptr));
			FormatAddrSymbol(s2, _countof(s2), (n > 2 ? frames[2] : nullptr));
			FormatAddrSymbol(s3, _countof(s3), (n > 3 ? frames[3] : nullptr));
			FormatAddrSymbol(s4, _countof(s4), (n > 4 ? frames[4] : nullptr));
			FormatAddrSymbol(s5, _countof(s5), (n > 5 ? frames[5] : nullptr));
			FormatAddrSymbol(s6, _countof(s6), (n > 6 ? frames[6] : nullptr));
			FormatAddrSymbol(s7, _countof(s7), (n > 7 ? frames[7] : nullptr));
		}

		core::LOG(core::log_inf,
			L"[D3D12] UploadMap WC (%s) ptr=%p size=%.1fMB allocBase=%p region=%.1fMB prot=0x%X stack: %s | %s | %s | %s | %s | %s | %s | %s",
			(Tag ? Tag : L"?"),
			MappedPtr,
			(double)SizeBytes / MB,
			mbi.AllocationBase,
			(double)(uint64_t)mbi.RegionSize / MB,
			(unsigned)mbi.Protect,
			s0, s1, s2, s3, s4, s5, s6, s7);

		sReentryGuard = false;
	}

	void D3D12UploadWCDiagnostics_OnAllocateLargePage(const wchar_t* Tag, std::size_t SizeBytes)
	{
		if (SizeBytes == 0)
			return;
		if (!RenderCore::D3D12RHI_ShouldEnableMemMon())
			return;

		// Aggregate and print at most once per second.
		static ULONGLONG sLastTick = 0;
		static volatile LONG64 sAggBytes = 0;
		static volatile LONG64 sAggCount = 0;

		::InterlockedAdd64(&sAggBytes, (LONG64)SizeBytes);
		::InterlockedIncrement64(&sAggCount);

		const ULONGLONG now = ::GetTickCount64();
		if (sLastTick == 0)
			sLastTick = now;
		if (now - sLastTick < 1000)
			return;
		sLastTick = now;

		const LONG64 aggBytes = ::InterlockedExchange64(&sAggBytes, 0);
		const LONG64 aggCount = ::InterlockedExchange64(&sAggCount, 0);
		if (aggBytes <= 0)
			return;

		static thread_local bool sReentryGuard = false;
		if (sReentryGuard)
			return;
		sReentryGuard = true;

		void* frames[16] = {};
		const USHORT n = ::RtlCaptureStackBackTrace(1, 16, frames, nullptr);

		wchar_t s0[256] = {}, s1[256] = {}, s2[256] = {}, s3[256] = {};
		wchar_t s4[256] = {}, s5[256] = {}, s6[256] = {}, s7[256] = {};
		if (n > 0)
		{
			FormatAddrSymbol(s0, _countof(s0), frames[0]);
			FormatAddrSymbol(s1, _countof(s1), (n > 1 ? frames[1] : nullptr));
			FormatAddrSymbol(s2, _countof(s2), (n > 2 ? frames[2] : nullptr));
			FormatAddrSymbol(s3, _countof(s3), (n > 3 ? frames[3] : nullptr));
			FormatAddrSymbol(s4, _countof(s4), (n > 4 ? frames[4] : nullptr));
			FormatAddrSymbol(s5, _countof(s5), (n > 5 ? frames[5] : nullptr));
			FormatAddrSymbol(s6, _countof(s6), (n > 6 ? frames[6] : nullptr));
			FormatAddrSymbol(s7, _countof(s7), (n > 7 ? frames[7] : nullptr));
		}

		const double MB = 1024.0 * 1024.0;
		core::LOG(core::log_inf,
			L"[D3D12] LargePageAlloc (%s) +%lld allocs +%.1fMB/s stack: %s | %s | %s | %s | %s | %s | %s | %s",
			(Tag ? Tag : L"?"),
			(long long)aggCount,
			(double)aggBytes / MB,
			s0, s1, s2, s3, s4, s5, s6, s7);

		sReentryGuard = false;
	}

	void D3D12UploadWCDiagnostics_OnCreateUploadCommittedBuffer(const wchar_t* Tag, std::size_t SizeBytes)
	{
		if (SizeBytes == 0)
			return;
		if (!RenderCore::D3D12RHI_ShouldEnableMemMon())
			return;

		// If deep memmon is enabled, print each call immediately with a stack so we don't miss
		// one-shot allocations (e.g. backing heaps created once at startup).
		if (RenderCore::D3D12RHI_ShouldEnableMemMonDeep())
		{
			static thread_local bool sReentryGuard = false;
			if (sReentryGuard)
				return;
			sReentryGuard = true;

			void* frames[16] = {};
			const USHORT n = ::RtlCaptureStackBackTrace(1, 16, frames, nullptr);

			wchar_t s0[256] = {}, s1[256] = {}, s2[256] = {}, s3[256] = {};
			wchar_t s4[256] = {}, s5[256] = {}, s6[256] = {}, s7[256] = {};
			if (n > 0)
			{
				FormatAddrSymbol(s0, _countof(s0), frames[0]);
				FormatAddrSymbol(s1, _countof(s1), (n > 1 ? frames[1] : nullptr));
				FormatAddrSymbol(s2, _countof(s2), (n > 2 ? frames[2] : nullptr));
				FormatAddrSymbol(s3, _countof(s3), (n > 3 ? frames[3] : nullptr));
				FormatAddrSymbol(s4, _countof(s4), (n > 4 ? frames[4] : nullptr));
				FormatAddrSymbol(s5, _countof(s5), (n > 5 ? frames[5] : nullptr));
				FormatAddrSymbol(s6, _countof(s6), (n > 6 ? frames[6] : nullptr));
				FormatAddrSymbol(s7, _countof(s7), (n > 7 ? frames[7] : nullptr));
			}

			const double MB = 1024.0 * 1024.0;
			core::LOG(core::log_inf,
				L"[D3D12] UploadCommittedBuffer (%s) +1 alloc %.1fMB stack: %s | %s | %s | %s | %s | %s | %s | %s",
				(Tag ? Tag : L"?"),
				(double)SizeBytes / MB,
				s0, s1, s2, s3, s4, s5, s6, s7);

			sReentryGuard = false;
			return;
		}

		// Aggregate and print at most once per second.
		static ULONGLONG sLastTick = 0;
		static volatile LONG64 sAggBytes = 0;
		static volatile LONG64 sAggCount = 0;

		::InterlockedAdd64(&sAggBytes, (LONG64)SizeBytes);
		::InterlockedIncrement64(&sAggCount);

		const ULONGLONG now = ::GetTickCount64();
		if (sLastTick == 0)
			sLastTick = now;
		if (now - sLastTick < 1000)
			return;
		sLastTick = now;

		const LONG64 aggBytes = ::InterlockedExchange64(&sAggBytes, 0);
		const LONG64 aggCount = ::InterlockedExchange64(&sAggCount, 0);
		if (aggBytes <= 0)
			return;

		static thread_local bool sReentryGuard = false;
		if (sReentryGuard)
			return;
		sReentryGuard = true;

		void* frames[16] = {};
		const USHORT n = ::RtlCaptureStackBackTrace(1, 16, frames, nullptr);

		wchar_t s0[256] = {}, s1[256] = {}, s2[256] = {}, s3[256] = {};
		wchar_t s4[256] = {}, s5[256] = {}, s6[256] = {}, s7[256] = {};
		if (n > 0)
		{
			FormatAddrSymbol(s0, _countof(s0), frames[0]);
			FormatAddrSymbol(s1, _countof(s1), (n > 1 ? frames[1] : nullptr));
			FormatAddrSymbol(s2, _countof(s2), (n > 2 ? frames[2] : nullptr));
			FormatAddrSymbol(s3, _countof(s3), (n > 3 ? frames[3] : nullptr));
			FormatAddrSymbol(s4, _countof(s4), (n > 4 ? frames[4] : nullptr));
			FormatAddrSymbol(s5, _countof(s5), (n > 5 ? frames[5] : nullptr));
			FormatAddrSymbol(s6, _countof(s6), (n > 6 ? frames[6] : nullptr));
			FormatAddrSymbol(s7, _countof(s7), (n > 7 ? frames[7] : nullptr));
		}

		const double MB = 1024.0 * 1024.0;
		core::LOG(core::log_inf,
			L"[D3D12] UploadCommittedBuffer (%s) +%lld allocs +%.1fMB/s stack: %s | %s | %s | %s | %s | %s | %s | %s",
			(Tag ? Tag : L"?"),
			(long long)aggCount,
			(double)aggBytes / MB,
			s0, s1, s2, s3, s4, s5, s6, s7);

		sReentryGuard = false;
	}
}

