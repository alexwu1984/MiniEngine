#include "D3D12/D3D12MemoryMonitor.h"

#include "D3D12/D3D12Adapter.h"
#include "RHI/RHI.h"
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
		return RenderCore::D3D12RHI_ShouldEnableMemMon();
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

		// HeapWalk + full VirtualQuery: very expensive; opt-in (d3d12_memmon_deep=1).
		if (!RenderCore::D3D12RHI_ShouldEnableMemMonDeep())
			return;

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

			core::LOG(core::log_inf,
				L"[D3D12] VMemPrivate WC Regions=%llu Bytes=%.1fMB",
				(unsigned long long)privWCRegions,
				(double)privWCBytes / MB);
		}
	}
}

