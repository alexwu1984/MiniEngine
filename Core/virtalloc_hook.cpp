#include "core/virtalloc_hook.h"
#include "core/commandline.h"
#include "core/logger.h"

#include <windows.h>
#include <DbgHelp.h>
#include <winternl.h>

#include "MinHook.h"

namespace core
{
	namespace
	{
		using PFN_NtAllocateVirtualMemory = NTSTATUS(NTAPI*)(HANDLE, PVOID*, ULONG_PTR, PSIZE_T, ULONG, ULONG);
		using PFN_NtFreeVirtualMemory = NTSTATUS(NTAPI*)(HANDLE, PVOID*, PSIZE_T, ULONG);
		using PFN_NtProtectVirtualMemory = NTSTATUS(NTAPI*)(HANDLE, PVOID*, PSIZE_T, ULONG, PULONG);
		using PFN_NtMapViewOfSection = NTSTATUS(NTAPI*)(HANDLE, HANDLE, PVOID*, ULONG_PTR, SIZE_T, PLARGE_INTEGER, PSIZE_T, DWORD /*SECTION_INHERIT*/, ULONG, ULONG);

		// Win10/11 VirtualAlloc2 path.
		typedef struct MEM_EXTENDED_PARAMETER
		{
			struct
			{
				ULONG64 Type : 8;
				ULONG64 Reserved : 56;
			} Type;
			ULONG64 Reserved2;
			union
			{
				ULONG64 ULong64;
				PVOID Pointer;
				SIZE_T Size;
				HANDLE Handle;
				ULONG ULong;
			} Value;
		} MEM_EXTENDED_PARAMETER, *PMEM_EXTENDED_PARAMETER;

		using PFN_NtAllocateVirtualMemoryEx = NTSTATUS(NTAPI*)(HANDLE, PVOID*, PSIZE_T, ULONG, ULONG, PMEM_EXTENDED_PARAMETER, ULONG);

		static PFN_NtAllocateVirtualMemory sRealNtAllocateVirtualMemory = nullptr;
		static PFN_NtFreeVirtualMemory sRealNtFreeVirtualMemory = nullptr;
		static PFN_NtProtectVirtualMemory sRealNtProtectVirtualMemory = nullptr;
		static PFN_NtAllocateVirtualMemoryEx sRealNtAllocateVirtualMemoryEx = nullptr;
		static PFN_NtMapViewOfSection sRealNtMapViewOfSection = nullptr;

		// KernelBase usermode path (many callers go through this instead of calling ntdll exports directly).
		using PFN_VirtualAlloc = LPVOID(WINAPI*)(LPVOID, SIZE_T, DWORD, DWORD);
		using PFN_VirtualProtect = BOOL(WINAPI*)(LPVOID, SIZE_T, DWORD, PDWORD);
		using PFN_VirtualAlloc2 = PVOID(WINAPI*)(HANDLE, PVOID, SIZE_T, ULONG, ULONG, PMEM_EXTENDED_PARAMETER, ULONG);

		static PFN_VirtualAlloc sRealVirtualAlloc = nullptr;
		static PFN_VirtualProtect sRealVirtualProtect = nullptr;
		static PFN_VirtualAlloc2 sRealVirtualAlloc2 = nullptr;

		// Some call sites import from Kernel32 or API-set DLLs (forwarders).
		static PFN_VirtualAlloc sRealVirtualAlloc_Kernel32 = nullptr;
		static PFN_VirtualProtect sRealVirtualProtect_Kernel32 = nullptr;
		static PFN_VirtualAlloc2 sRealVirtualAlloc2_Kernel32 = nullptr;

		static PFN_VirtualAlloc sRealVirtualAlloc_ApiSet = nullptr;
		static PFN_VirtualProtect sRealVirtualProtect_ApiSet = nullptr;
		static PFN_VirtualAlloc2 sRealVirtualAlloc2_ApiSet = nullptr;

		// Aggregate to avoid log spam when commit/protect happens in small chunks.
		static volatile LONG64 sWcCommitBytes = 0;
		static volatile LONG64 sWcProtectBytes = 0;

		// D3DKMT (gdi32) path: some GPU driver allocations are requested via KMT and may not
		// show up as usermode VirtualAlloc/NtAllocateVirtualMemory call sites.
		using PFN_D3DKMTCreateAllocation = NTSTATUS(APIENTRY*)(void* /*D3DKMT_CREATEALLOCATION* */);
		using PFN_D3DKMTDestroyAllocation = NTSTATUS(APIENTRY*)(void* /*D3DKMT_DESTROYALLOCATION* */);
		using PFN_D3DKMTSubmitCommand = NTSTATUS(APIENTRY*)(void* /*D3DKMT_SUBMITCOMMAND* */);

		static PFN_D3DKMTCreateAllocation sRealD3DKMTCreateAllocation = nullptr;
		static PFN_D3DKMTDestroyAllocation sRealD3DKMTDestroyAllocation = nullptr;
		static PFN_D3DKMTSubmitCommand sRealD3DKMTSubmitCommand = nullptr;

		static void LogD3dkmtOncePerSecond(const wchar_t* Tag, void* ArgsPtr, NTSTATUS Status)
		{
			static thread_local bool sReentryGuard = false;
			if (sReentryGuard)
				return;
			sReentryGuard = true;

			static ULONGLONG sLastTick = 0;
			const ULONGLONG now = ::GetTickCount64();
			const ULONGLONG kCooldownMs = 250;
			if (now - sLastTick < kCooldownMs)
			{
				sReentryGuard = false;
				return;
			}
			sLastTick = now;

			void* frames[32] = {};
			USHORT n = ::RtlCaptureStackBackTrace(1, 32, frames, nullptr);
			core::LOG(core::log_inf,
					  L"[WC_HOOK] %s args=%p st=0x%08x frames=%u (enable wc_hook_stacks=1 for symbols)",
					  Tag, ArgsPtr, (unsigned)Status, (unsigned)n);

			if (core::CommandLine::Get().GetName("wc_hook_stacks"))
			{
				HANDLE proc = ::GetCurrentProcess();
				static bool sSymInited = false;
				if (!sSymInited)
				{
					::SymSetOptions(SYMOPT_UNDNAME | SYMOPT_DEFERRED_LOADS | SYMOPT_LOAD_LINES);
					::SymInitialize(proc, nullptr, FALSE);
					// Ensure the main module's symbols are loaded (invade=FALSE doesn't guarantee it).
					wchar_t modulePathW[MAX_PATH] = {};
					if (::GetModuleFileNameW(nullptr, modulePathW, (DWORD)_countof(modulePathW)) > 0)
					{
						HMODULE hMod = ::GetModuleHandleW(nullptr);
						::SymLoadModuleExW(proc, nullptr, modulePathW, nullptr, (DWORD64)(uintptr_t)hMod, 0, nullptr, 0);
					}
					sSymInited = true;
				}
				for (USHORT i = 0; i < n && i < 10; ++i)
				{
					DWORD64 addr = (DWORD64)frames[i];
					char buf[sizeof(SYMBOL_INFO) + MAX_SYM_NAME] = {};
					SYMBOL_INFO* sym = (SYMBOL_INFO*)buf;
					sym->SizeOfStruct = sizeof(SYMBOL_INFO);
					sym->MaxNameLen = MAX_SYM_NAME;
					DWORD64 disp = 0;
					if (::SymFromAddr(proc, addr, &disp, sym))
						core::LOG(core::log_inf, L"[WC_HOOK]   #%u %S +0x%llx", (unsigned)i, sym->Name, (unsigned long long)disp);
					else
						core::LOG(core::log_inf, L"[WC_HOOK]   #%u 0x%llx", (unsigned)i, (unsigned long long)addr);
				}
			}

			sReentryGuard = false;
		}

		static void LogStackOncePerSecond(const wchar_t* Tag, void* Base, SIZE_T Size, DWORD Protect)
		{
			static thread_local bool sReentryGuard = false;
			if (sReentryGuard)
				return;
			sReentryGuard = true;

			// Pull aggregate WC deltas first.  This avoids "missing the hotspot" when the first
			// large mapping is a DLL (non-WC) and consumes the global once-per-second budget.
			const LONG64 wcCommit = ::InterlockedExchange64(&sWcCommitBytes, 0);
			const LONG64 wcProtect = ::InterlockedExchange64(&sWcProtectBytes, 0);

			// Filter: for non-WC large mappings, don't print stacks (typically DLL/image mappings).
			// We still keep the aggregate counters for diagnostics, but stacks should focus on WC.
			const bool bIsWC = (Protect & PAGE_WRITECOMBINE) != 0;
			if (!bIsWC && wcCommit == 0 && wcProtect == 0)
			{
				sReentryGuard = false;
				return;
			}

			// Throttle per-category, but allow immediate logs when WC deltas are large.
			const ULONGLONG now = ::GetTickCount64();
			auto SlotForTag = [](const wchar_t* T) -> int
			{
				if (!T) return 0;
				if (wcsstr(T, L"NtAllocateVirtualMemory") != nullptr) return 0;
				if (wcsstr(T, L"NtAllocateVirtualMemoryEx") != nullptr) return 0;
				if (wcsstr(T, L"NtProtectVirtualMemory") != nullptr) return 1;
				if (wcsstr(T, L"NtMapViewOfSection") != nullptr) return 2;
				return 3;
			};

			static ULONGLONG sLastTickBySlot[4] = {};
			const int slot = SlotForTag(Tag);

			// If we saw >=32MB of WC activity since last log, always print (hot path).
			const LONG64 kHotBytes = (LONG64)(32ull * 1024ull * 1024ull);
			const bool bHot = (wcCommit >= kHotBytes) || (wcProtect >= kHotBytes) || (Size >= (32ull << 20) && bIsWC);

			// Otherwise, keep stacks at a reasonable rate per category.
			const ULONGLONG kCooldownMs = 250;
			if (!bHot && (now - sLastTickBySlot[slot] < kCooldownMs))
			{
				sReentryGuard = false;
				return;
			}
			sLastTickBySlot[slot] = now;

			void* frames[32] = {};
			USHORT n = ::RtlCaptureStackBackTrace(1, 32, frames, nullptr);
			core::LOG(core::log_inf,
					  L"[WC_HOOK] %s base=%p size=%.1fMB protect=0x%08x frames=%u | agg(WC Commit=%.1fMB Protect=%.1fMB)/s",
					  Tag, Base, (double)Size / (1024.0 * 1024.0), (unsigned)Protect, (unsigned)n,
					  (double)wcCommit / (1024.0 * 1024.0), (double)wcProtect / (1024.0 * 1024.0));

			// Symbolization is optional and can allocate heavily (DbgHelp). Keep it behind a flag.
			if (core::CommandLine::Get().GetName("wc_hook_stacks"))
			{
				HANDLE proc = ::GetCurrentProcess();
				static bool sSymInited = false;
				if (!sSymInited)
				{
					::SymSetOptions(SYMOPT_UNDNAME | SYMOPT_DEFERRED_LOADS | SYMOPT_LOAD_LINES);
					::SymInitialize(proc, nullptr, FALSE);
					// Ensure the main module's symbols are loaded (invade=FALSE doesn't guarantee it).
					wchar_t modulePathW[MAX_PATH] = {};
					if (::GetModuleFileNameW(nullptr, modulePathW, (DWORD)_countof(modulePathW)) > 0)
					{
						HMODULE hMod = ::GetModuleHandleW(nullptr);
						::SymLoadModuleExW(proc, nullptr, modulePathW, nullptr, (DWORD64)(uintptr_t)hMod, 0, nullptr, 0);
					}
					sSymInited = true;
				}
				for (USHORT i = 0; i < n && i < 10; ++i)
				{
					DWORD64 addr = (DWORD64)frames[i];
					char buf[sizeof(SYMBOL_INFO) + MAX_SYM_NAME] = {};
					SYMBOL_INFO* sym = (SYMBOL_INFO*)buf;
					sym->SizeOfStruct = sizeof(SYMBOL_INFO);
					sym->MaxNameLen = MAX_SYM_NAME;
					DWORD64 disp = 0;
					if (::SymFromAddr(proc, addr, &disp, sym))
					{
						core::LOG(core::log_inf, L"[WC_HOOK]   #%u %S +0x%llx", (unsigned)i, sym->Name, (unsigned long long)disp);
					}
					else
					{
						core::LOG(core::log_inf, L"[WC_HOOK]   #%u 0x%llx", (unsigned)i, (unsigned long long)addr);
					}
				}
			}

			sReentryGuard = false;
		}

		static NTSTATUS APIENTRY Hook_D3DKMTCreateAllocation(void* Args)
		{
			NTSTATUS st = sRealD3DKMTCreateAllocation ? sRealD3DKMTCreateAllocation(Args) : (NTSTATUS)0xC0000002 /*STATUS_NOT_IMPLEMENTED*/;
			LogD3dkmtOncePerSecond(L"gdi32!D3DKMTCreateAllocation", Args, st);
			return st;
		}

		static NTSTATUS APIENTRY Hook_D3DKMTDestroyAllocation(void* Args)
		{
			NTSTATUS st = sRealD3DKMTDestroyAllocation ? sRealD3DKMTDestroyAllocation(Args) : (NTSTATUS)0xC0000002 /*STATUS_NOT_IMPLEMENTED*/;
			LogD3dkmtOncePerSecond(L"gdi32!D3DKMTDestroyAllocation", Args, st);
			return st;
		}

		static NTSTATUS APIENTRY Hook_D3DKMTSubmitCommand(void* Args)
		{
			NTSTATUS st = sRealD3DKMTSubmitCommand ? sRealD3DKMTSubmitCommand(Args) : (NTSTATUS)0xC0000002 /*STATUS_NOT_IMPLEMENTED*/;
			LogD3dkmtOncePerSecond(L"gdi32!D3DKMTSubmitCommand", Args, st);
			return st;
		}

		static NTSTATUS NTAPI Hook_NtAllocateVirtualMemory(
			HANDLE ProcessHandle,
			PVOID* BaseAddress,
			ULONG_PTR ZeroBits,
			PSIZE_T RegionSize,
			ULONG AllocationType,
			ULONG Protect)
		{
			NTSTATUS st = sRealNtAllocateVirtualMemory(ProcessHandle, BaseAddress, ZeroBits, RegionSize, AllocationType, Protect);
			if (NT_SUCCESS(st) && (AllocationType & MEM_COMMIT) && BaseAddress && RegionSize && *RegionSize)
			{
				// Don't rely on ProcessHandle equality: callers may pass a real process handle.
				// Also, some paths may allocate RW then flip to WC; we still want the alloc stack.
				const SIZE_T sz = *RegionSize;

				// Some drivers/runtime paths request RW but end up with WC (0x404) at the VAD level.
				// Trust the *result* protection, not only the input Protect.
				MEMORY_BASIC_INFORMATION mbi = {};
				if (*BaseAddress && ::VirtualQuery(*BaseAddress, &mbi, sizeof(mbi)) == sizeof(mbi))
				{
					if (mbi.State == MEM_COMMIT && (mbi.Protect & PAGE_WRITECOMBINE))
						::InterlockedAdd64(&sWcCommitBytes, (LONG64)sz);
				}

				if ((Protect & PAGE_WRITECOMBINE) && sz >= (1ull << 20))
				{
					LogStackOncePerSecond(L"NtAllocateVirtualMemory(COMMIT,WC)", *BaseAddress, sz, (DWORD)Protect);
				}
				else if (*BaseAddress && (sz >= (1ull << 20)) && ::VirtualQuery(*BaseAddress, &mbi, sizeof(mbi)) == sizeof(mbi) &&
						 (mbi.State == MEM_COMMIT) && (mbi.Protect & PAGE_WRITECOMBINE))
				{
					LogStackOncePerSecond(L"NtAllocateVirtualMemory(COMMIT,WC-result)", *BaseAddress, sz, (DWORD)mbi.Protect);
				}
				else if (sz >= (32ull << 20))
				{
					// Heuristic: these 32MB blocks are exactly what we see in VMemPrivate WC top list.
					// Log alloc site even if Protect isn't WC yet.
					LogStackOncePerSecond(L"NtAllocateVirtualMemory(COMMIT,>=32MB)", *BaseAddress, sz, (DWORD)Protect);
				}
			}
			return st;
		}

		static NTSTATUS NTAPI Hook_NtFreeVirtualMemory(
			HANDLE ProcessHandle,
			PVOID* BaseAddress,
			PSIZE_T RegionSize,
			ULONG FreeType)
		{
			return sRealNtFreeVirtualMemory(ProcessHandle, BaseAddress, RegionSize, FreeType);
		}

		static NTSTATUS NTAPI Hook_NtProtectVirtualMemory(
			HANDLE ProcessHandle,
			PVOID* BaseAddress,
			PSIZE_T RegionSize,
			ULONG NewProtect,
			PULONG OldProtect)
		{
			NTSTATUS st = sRealNtProtectVirtualMemory(ProcessHandle, BaseAddress, RegionSize, NewProtect, OldProtect);
			if (NT_SUCCESS(st) && BaseAddress && RegionSize && *RegionSize)
			{
				const SIZE_T sz = *RegionSize;
				MEMORY_BASIC_INFORMATION mbi = {};
				if (*BaseAddress && ::VirtualQuery(*BaseAddress, &mbi, sizeof(mbi)) == sizeof(mbi))
				{
					if (mbi.State == MEM_COMMIT && (mbi.Protect & PAGE_WRITECOMBINE))
						::InterlockedAdd64(&sWcProtectBytes, (LONG64)sz);
				}

				// Primary: explicit WC protection.
				if ((NewProtect & PAGE_WRITECOMBINE) && sz >= (1ull << 20))
				{
					LogStackOncePerSecond(L"NtProtectVirtualMemory(WC)", *BaseAddress, sz, (DWORD)NewProtect);
				}
				else if (*BaseAddress && (sz >= (1ull << 20)) && ::VirtualQuery(*BaseAddress, &mbi, sizeof(mbi)) == sizeof(mbi) &&
						 (mbi.State == MEM_COMMIT) && (mbi.Protect & PAGE_WRITECOMBINE))
				{
					LogStackOncePerSecond(L"NtProtectVirtualMemory(WC-result)", *BaseAddress, sz, (DWORD)mbi.Protect);
				}
				// Fallback: large protect changes (driver may use intermediate flags).
				else if (sz >= (32ull << 20))
				{
					LogStackOncePerSecond(L"NtProtectVirtualMemory(>=32MB)", *BaseAddress, sz, (DWORD)NewProtect);
				}
			}
			return st;
		}

		static NTSTATUS NTAPI Hook_NtAllocateVirtualMemoryEx(
			HANDLE ProcessHandle,
			PVOID* BaseAddress,
			PSIZE_T RegionSize,
			ULONG AllocationType,
			ULONG Protect,
			PMEM_EXTENDED_PARAMETER ExtendedParameters,
			ULONG ExtendedParameterCount)
		{
			NTSTATUS st = sRealNtAllocateVirtualMemoryEx(ProcessHandle, BaseAddress, RegionSize, AllocationType, Protect, ExtendedParameters, ExtendedParameterCount);
			if (NT_SUCCESS(st) && (AllocationType & MEM_COMMIT) && BaseAddress && RegionSize && *RegionSize)
			{
				const SIZE_T sz = *RegionSize;
				MEMORY_BASIC_INFORMATION mbi = {};
				if (*BaseAddress && ::VirtualQuery(*BaseAddress, &mbi, sizeof(mbi)) == sizeof(mbi))
				{
					if (mbi.State == MEM_COMMIT && (mbi.Protect & PAGE_WRITECOMBINE))
						::InterlockedAdd64(&sWcCommitBytes, (LONG64)sz);
				}
				if ((Protect & PAGE_WRITECOMBINE) && sz >= (1ull << 20))
				{
					LogStackOncePerSecond(L"NtAllocateVirtualMemoryEx(COMMIT,WC)", *BaseAddress, sz, (DWORD)Protect);
				}
				else if (*BaseAddress && (sz >= (1ull << 20)) && ::VirtualQuery(*BaseAddress, &mbi, sizeof(mbi)) == sizeof(mbi) &&
						 (mbi.State == MEM_COMMIT) && (mbi.Protect & PAGE_WRITECOMBINE))
				{
					LogStackOncePerSecond(L"NtAllocateVirtualMemoryEx(COMMIT,WC-result)", *BaseAddress, sz, (DWORD)mbi.Protect);
				}
				else if (sz >= (32ull << 20))
				{
					LogStackOncePerSecond(L"NtAllocateVirtualMemoryEx(COMMIT,>=32MB)", *BaseAddress, sz, (DWORD)Protect);
				}
			}
			return st;
		}

		static NTSTATUS NTAPI Hook_NtMapViewOfSection(
			HANDLE SectionHandle,
			HANDLE ProcessHandle,
			PVOID* BaseAddress,
			ULONG_PTR ZeroBits,
			SIZE_T CommitSize,
			PLARGE_INTEGER SectionOffset,
			PSIZE_T ViewSize,
			DWORD InheritDisposition,
			ULONG AllocationType,
			ULONG Win32Protect)
		{
			NTSTATUS st = sRealNtMapViewOfSection(SectionHandle, ProcessHandle, BaseAddress, ZeroBits, CommitSize, SectionOffset, ViewSize, InheritDisposition, AllocationType, Win32Protect);
			if (NT_SUCCESS(st) && BaseAddress && ViewSize && *ViewSize)
			{
				const SIZE_T sz = *ViewSize;
				MEMORY_BASIC_INFORMATION mbi = {};
				if (*BaseAddress && ::VirtualQuery(*BaseAddress, &mbi, sizeof(mbi)) == sizeof(mbi))
				{
					if (mbi.State == MEM_COMMIT && (mbi.Protect & PAGE_WRITECOMBINE))
						::InterlockedAdd64(&sWcCommitBytes, (LONG64)sz);
				}
				if ((Win32Protect & PAGE_WRITECOMBINE) && sz >= (1ull << 20))
				{
					LogStackOncePerSecond(L"NtMapViewOfSection(WC)", *BaseAddress, sz, (DWORD)Win32Protect);
				}
				else if (*BaseAddress && (sz >= (1ull << 20)) && ::VirtualQuery(*BaseAddress, &mbi, sizeof(mbi)) == sizeof(mbi) &&
						 (mbi.State == MEM_COMMIT) && (mbi.Protect & PAGE_WRITECOMBINE))
				{
					LogStackOncePerSecond(L"NtMapViewOfSection(WC-result)", *BaseAddress, sz, (DWORD)mbi.Protect);
				}
				else if (sz >= (32ull << 20))
				{
					LogStackOncePerSecond(L"NtMapViewOfSection(>=32MB)", *BaseAddress, sz, (DWORD)Win32Protect);
				}
			}
			return st;
		}

		static LPVOID WINAPI Hook_VirtualAlloc(LPVOID lpAddress, SIZE_T dwSize, DWORD flAllocationType, DWORD flProtect)
		{
			LPVOID p = sRealVirtualAlloc ? sRealVirtualAlloc(lpAddress, dwSize, flAllocationType, flProtect) : nullptr;
			if (p && (flAllocationType & MEM_COMMIT) && dwSize)
			{
				MEMORY_BASIC_INFORMATION mbi = {};
				if (::VirtualQuery(p, &mbi, sizeof(mbi)) == sizeof(mbi))
				{
					if (mbi.State == MEM_COMMIT && (mbi.Protect & PAGE_WRITECOMBINE))
						::InterlockedAdd64(&sWcCommitBytes, (LONG64)dwSize);
				}

				if (((flProtect & PAGE_WRITECOMBINE) != 0) && dwSize >= (1ull << 20))
				{
					LogStackOncePerSecond(L"VirtualAlloc(COMMIT,WC)", p, dwSize, flProtect);
				}
				else if (dwSize >= (32ull << 20))
				{
					LogStackOncePerSecond(L"VirtualAlloc(COMMIT,>=32MB)", p, dwSize, flProtect);
				}
			}
			return p;
		}

		static LPVOID WINAPI Hook_VirtualAlloc_Kernel32(LPVOID lpAddress, SIZE_T dwSize, DWORD flAllocationType, DWORD flProtect)
		{
			LPVOID p = sRealVirtualAlloc_Kernel32 ? sRealVirtualAlloc_Kernel32(lpAddress, dwSize, flAllocationType, flProtect) : nullptr;
			if (p && (flAllocationType & MEM_COMMIT) && dwSize)
			{
				if (((flProtect & PAGE_WRITECOMBINE) != 0) && dwSize >= (1ull << 20))
					LogStackOncePerSecond(L"Kernel32!VirtualAlloc(COMMIT,WC)", p, dwSize, flProtect);
				else if (dwSize >= (32ull << 20))
					LogStackOncePerSecond(L"Kernel32!VirtualAlloc(COMMIT,>=32MB)", p, dwSize, flProtect);
			}
			return p;
		}

		static LPVOID WINAPI Hook_VirtualAlloc_ApiSet(LPVOID lpAddress, SIZE_T dwSize, DWORD flAllocationType, DWORD flProtect)
		{
			LPVOID p = sRealVirtualAlloc_ApiSet ? sRealVirtualAlloc_ApiSet(lpAddress, dwSize, flAllocationType, flProtect) : nullptr;
			if (p && (flAllocationType & MEM_COMMIT) && dwSize)
			{
				if (((flProtect & PAGE_WRITECOMBINE) != 0) && dwSize >= (1ull << 20))
					LogStackOncePerSecond(L"apiset!VirtualAlloc(COMMIT,WC)", p, dwSize, flProtect);
				else if (dwSize >= (32ull << 20))
					LogStackOncePerSecond(L"apiset!VirtualAlloc(COMMIT,>=32MB)", p, dwSize, flProtect);
			}
			return p;
		}

		static BOOL WINAPI Hook_VirtualProtect(LPVOID lpAddress, SIZE_T dwSize, DWORD flNewProtect, PDWORD lpflOldProtect)
		{
			BOOL ok = sRealVirtualProtect ? sRealVirtualProtect(lpAddress, dwSize, flNewProtect, lpflOldProtect) : FALSE;
			if (ok && lpAddress && dwSize)
			{
				MEMORY_BASIC_INFORMATION mbi = {};
				if (::VirtualQuery(lpAddress, &mbi, sizeof(mbi)) == sizeof(mbi))
				{
					if (mbi.State == MEM_COMMIT && (mbi.Protect & PAGE_WRITECOMBINE))
						::InterlockedAdd64(&sWcProtectBytes, (LONG64)dwSize);
				}

				if (((flNewProtect & PAGE_WRITECOMBINE) != 0) && dwSize >= (1ull << 20))
				{
					LogStackOncePerSecond(L"VirtualProtect(WC)", lpAddress, dwSize, flNewProtect);
				}
				else if (dwSize >= (32ull << 20))
				{
					LogStackOncePerSecond(L"VirtualProtect(>=32MB)", lpAddress, dwSize, flNewProtect);
				}
			}
			return ok;
		}

		static BOOL WINAPI Hook_VirtualProtect_Kernel32(LPVOID lpAddress, SIZE_T dwSize, DWORD flNewProtect, PDWORD lpflOldProtect)
		{
			BOOL ok = sRealVirtualProtect_Kernel32 ? sRealVirtualProtect_Kernel32(lpAddress, dwSize, flNewProtect, lpflOldProtect) : FALSE;
			if (ok && lpAddress && dwSize)
			{
				if (((flNewProtect & PAGE_WRITECOMBINE) != 0) && dwSize >= (1ull << 20))
					LogStackOncePerSecond(L"Kernel32!VirtualProtect(WC)", lpAddress, dwSize, flNewProtect);
				else if (dwSize >= (32ull << 20))
					LogStackOncePerSecond(L"Kernel32!VirtualProtect(>=32MB)", lpAddress, dwSize, flNewProtect);
			}
			return ok;
		}

		static BOOL WINAPI Hook_VirtualProtect_ApiSet(LPVOID lpAddress, SIZE_T dwSize, DWORD flNewProtect, PDWORD lpflOldProtect)
		{
			BOOL ok = sRealVirtualProtect_ApiSet ? sRealVirtualProtect_ApiSet(lpAddress, dwSize, flNewProtect, lpflOldProtect) : FALSE;
			if (ok && lpAddress && dwSize)
			{
				if (((flNewProtect & PAGE_WRITECOMBINE) != 0) && dwSize >= (1ull << 20))
					LogStackOncePerSecond(L"apiset!VirtualProtect(WC)", lpAddress, dwSize, flNewProtect);
				else if (dwSize >= (32ull << 20))
					LogStackOncePerSecond(L"apiset!VirtualProtect(>=32MB)", lpAddress, dwSize, flNewProtect);
			}
			return ok;
		}

		static PVOID WINAPI Hook_VirtualAlloc2(
			HANDLE ProcessHandle, PVOID BaseAddress, SIZE_T Size, ULONG AllocationType, ULONG PageProtection,
			PMEM_EXTENDED_PARAMETER ExtendedParameters, ULONG ExtendedParameterCount)
		{
			PVOID p = sRealVirtualAlloc2 ? sRealVirtualAlloc2(ProcessHandle, BaseAddress, Size, AllocationType, PageProtection, ExtendedParameters, ExtendedParameterCount) : nullptr;
			if (p && (AllocationType & MEM_COMMIT) && Size)
			{
				MEMORY_BASIC_INFORMATION mbi = {};
				if (::VirtualQuery(p, &mbi, sizeof(mbi)) == sizeof(mbi))
				{
					if (mbi.State == MEM_COMMIT && (mbi.Protect & PAGE_WRITECOMBINE))
						::InterlockedAdd64(&sWcCommitBytes, (LONG64)Size);
				}

				if (((PageProtection & PAGE_WRITECOMBINE) != 0) && Size >= (1ull << 20))
				{
					LogStackOncePerSecond(L"VirtualAlloc2(COMMIT,WC)", p, Size, (DWORD)PageProtection);
				}
				else if (Size >= (32ull << 20))
				{
					LogStackOncePerSecond(L"VirtualAlloc2(COMMIT,>=32MB)", p, Size, (DWORD)PageProtection);
				}
			}
			return p;
		}

		static PVOID WINAPI Hook_VirtualAlloc2_Kernel32(
			HANDLE ProcessHandle, PVOID BaseAddress, SIZE_T Size, ULONG AllocationType, ULONG PageProtection,
			PMEM_EXTENDED_PARAMETER ExtendedParameters, ULONG ExtendedParameterCount)
		{
			PVOID p = sRealVirtualAlloc2_Kernel32 ? sRealVirtualAlloc2_Kernel32(ProcessHandle, BaseAddress, Size, AllocationType, PageProtection, ExtendedParameters, ExtendedParameterCount) : nullptr;
			if (p && (AllocationType & MEM_COMMIT) && Size)
			{
				if (((PageProtection & PAGE_WRITECOMBINE) != 0) && Size >= (1ull << 20))
					LogStackOncePerSecond(L"Kernel32!VirtualAlloc2(COMMIT,WC)", p, Size, (DWORD)PageProtection);
				else if (Size >= (32ull << 20))
					LogStackOncePerSecond(L"Kernel32!VirtualAlloc2(COMMIT,>=32MB)", p, Size, (DWORD)PageProtection);
			}
			return p;
		}

		static PVOID WINAPI Hook_VirtualAlloc2_ApiSet(
			HANDLE ProcessHandle, PVOID BaseAddress, SIZE_T Size, ULONG AllocationType, ULONG PageProtection,
			PMEM_EXTENDED_PARAMETER ExtendedParameters, ULONG ExtendedParameterCount)
		{
			PVOID p = sRealVirtualAlloc2_ApiSet ? sRealVirtualAlloc2_ApiSet(ProcessHandle, BaseAddress, Size, AllocationType, PageProtection, ExtendedParameters, ExtendedParameterCount) : nullptr;
			if (p && (AllocationType & MEM_COMMIT) && Size)
			{
				if (((PageProtection & PAGE_WRITECOMBINE) != 0) && Size >= (1ull << 20))
					LogStackOncePerSecond(L"apiset!VirtualAlloc2(COMMIT,WC)", p, Size, (DWORD)PageProtection);
				else if (Size >= (32ull << 20))
					LogStackOncePerSecond(L"apiset!VirtualAlloc2(COMMIT,>=32MB)", p, Size, (DWORD)PageProtection);
			}
			return p;
		}
	}

	void InitVirtualAllocHookIfRequested()
	{
		int on = 0;
		core::CommandLine::Get().GetInteger("wc_hook", on);
		if (on <= 0)
			return;

		MH_STATUS st = MH_Initialize();
		if (st != MH_OK && st != MH_ERROR_ALREADY_INITIALIZED)
		{
			core::LOG(core::log_inf, L"[WC_HOOK] MH_Initialize failed: %S", MH_StatusToString(st));
			return;
		}

		st = MH_CreateHookApi(L"ntdll.dll", "NtFreeVirtualMemory", (LPVOID)&Hook_NtFreeVirtualMemory, (LPVOID*)&sRealNtFreeVirtualMemory);
		if (st != MH_OK)
		{
			core::LOG(core::log_inf, L"[WC_HOOK] hook NtFreeVirtualMemory failed: %S", MH_StatusToString(st));
			return;
		}

		st = MH_CreateHookApi(L"ntdll.dll", "NtAllocateVirtualMemory", (LPVOID)&Hook_NtAllocateVirtualMemory, (LPVOID*)&sRealNtAllocateVirtualMemory);
		if (st != MH_OK)
		{
			core::LOG(core::log_inf, L"[WC_HOOK] hook NtAllocateVirtualMemory failed: %S", MH_StatusToString(st));
			return;
		}

		st = MH_CreateHookApi(L"ntdll.dll", "NtProtectVirtualMemory", (LPVOID)&Hook_NtProtectVirtualMemory, (LPVOID*)&sRealNtProtectVirtualMemory);
		if (st != MH_OK)
		{
			core::LOG(core::log_inf, L"[WC_HOOK] hook NtProtectVirtualMemory failed: %S", MH_StatusToString(st));
			return;
		}

		// Optional: VirtualAlloc2 path (Win10/11).
		st = MH_CreateHookApi(L"ntdll.dll", "NtAllocateVirtualMemoryEx", (LPVOID)&Hook_NtAllocateVirtualMemoryEx, (LPVOID*)&sRealNtAllocateVirtualMemoryEx);
		if (st != MH_OK)
		{
			core::LOG(core::log_inf, L"[WC_HOOK] hook NtAllocateVirtualMemoryEx failed: %S", MH_StatusToString(st));
			// Don't hard fail; some OS builds may not export it.
			sRealNtAllocateVirtualMemoryEx = nullptr;
		}

		// Optional: section mapping path.
		st = MH_CreateHookApi(L"ntdll.dll", "NtMapViewOfSection", (LPVOID)&Hook_NtMapViewOfSection, (LPVOID*)&sRealNtMapViewOfSection);
		if (st != MH_OK)
		{
			core::LOG(core::log_inf, L"[WC_HOOK] hook NtMapViewOfSection failed: %S", MH_StatusToString(st));
			sRealNtMapViewOfSection = nullptr;
		}

		// Optional: hook KernelBase VirtualAlloc/VirtualAlloc2/VirtualProtect (common usermode entry points).
		st = MH_CreateHookApi(L"KernelBase.dll", "VirtualAlloc", (LPVOID)&Hook_VirtualAlloc, (LPVOID*)&sRealVirtualAlloc);
		if (st != MH_OK)
		{
			core::LOG(core::log_inf, L"[WC_HOOK] hook KernelBase!VirtualAlloc failed: %S", MH_StatusToString(st));
			sRealVirtualAlloc = nullptr;
		}
		st = MH_CreateHookApi(L"KernelBase.dll", "VirtualProtect", (LPVOID)&Hook_VirtualProtect, (LPVOID*)&sRealVirtualProtect);
		if (st != MH_OK)
		{
			core::LOG(core::log_inf, L"[WC_HOOK] hook KernelBase!VirtualProtect failed: %S", MH_StatusToString(st));
			sRealVirtualProtect = nullptr;
		}
		st = MH_CreateHookApi(L"KernelBase.dll", "VirtualAlloc2", (LPVOID)&Hook_VirtualAlloc2, (LPVOID*)&sRealVirtualAlloc2);
		if (st != MH_OK)
		{
			// Older builds may not export VirtualAlloc2.
			sRealVirtualAlloc2 = nullptr;
		}

		// Optional: Kernel32 forwarders.
		st = MH_CreateHookApi(L"Kernel32.dll", "VirtualAlloc", (LPVOID)&Hook_VirtualAlloc_Kernel32, (LPVOID*)&sRealVirtualAlloc_Kernel32);
		if (st != MH_OK)
			sRealVirtualAlloc_Kernel32 = nullptr;
		st = MH_CreateHookApi(L"Kernel32.dll", "VirtualProtect", (LPVOID)&Hook_VirtualProtect_Kernel32, (LPVOID*)&sRealVirtualProtect_Kernel32);
		if (st != MH_OK)
			sRealVirtualProtect_Kernel32 = nullptr;
		st = MH_CreateHookApi(L"Kernel32.dll", "VirtualAlloc2", (LPVOID)&Hook_VirtualAlloc2_Kernel32, (LPVOID*)&sRealVirtualAlloc2_Kernel32);
		if (st != MH_OK)
			sRealVirtualAlloc2_Kernel32 = nullptr;

		// Optional: API-set name (some import tables reference this directly).
		st = MH_CreateHookApi(L"api-ms-win-core-memory-l1-1-0.dll", "VirtualAlloc", (LPVOID)&Hook_VirtualAlloc_ApiSet, (LPVOID*)&sRealVirtualAlloc_ApiSet);
		if (st != MH_OK)
			sRealVirtualAlloc_ApiSet = nullptr;
		st = MH_CreateHookApi(L"api-ms-win-core-memory-l1-1-0.dll", "VirtualProtect", (LPVOID)&Hook_VirtualProtect_ApiSet, (LPVOID*)&sRealVirtualProtect_ApiSet);
		if (st != MH_OK)
			sRealVirtualProtect_ApiSet = nullptr;
		st = MH_CreateHookApi(L"api-ms-win-core-memory-l1-1-0.dll", "VirtualAlloc2", (LPVOID)&Hook_VirtualAlloc2_ApiSet, (LPVOID*)&sRealVirtualAlloc2_ApiSet);
		if (st != MH_OK)
			sRealVirtualAlloc2_ApiSet = nullptr;

		st = MH_EnableHook(MH_ALL_HOOKS);
		if (st != MH_OK)
		{
			core::LOG(core::log_inf, L"[WC_HOOK] MH_EnableHook failed: %S", MH_StatusToString(st));
			return;
		}

		// Optional: D3DKMT hooks. These can help attribute driver/kernel allocations that don't
		// surface as usermode VirtualAlloc/NtAllocateVirtualMemory call stacks.
		//
		// Note: on some Win10/11 builds these exports live in win32u.dll (syscall user stub),
		// not gdi32.dll. We try both and log success/failure explicitly.
		int d3dkmtOn = 0;
		core::CommandLine::Get().GetInteger("wc_hook_d3dkmt", d3dkmtOn);
		if (d3dkmtOn > 0)
		{
			auto TryHookD3dkmtIn = [](const wchar_t* Module) -> void
			{
				MH_STATUS hk = MH_CreateHookApi(Module, "D3DKMTCreateAllocation", (LPVOID)&Hook_D3DKMTCreateAllocation, (LPVOID*)&sRealD3DKMTCreateAllocation);
				core::LOG(core::log_inf, L"[WC_HOOK] hook %s!D3DKMTCreateAllocation %s", Module, hk == MH_OK ? L"ok" : L"failed");
				if (hk != MH_OK)
					core::LOG(core::log_inf, L"[WC_HOOK]   reason: %S", MH_StatusToString(hk));

				hk = MH_CreateHookApi(Module, "D3DKMTDestroyAllocation", (LPVOID)&Hook_D3DKMTDestroyAllocation, (LPVOID*)&sRealD3DKMTDestroyAllocation);
				core::LOG(core::log_inf, L"[WC_HOOK] hook %s!D3DKMTDestroyAllocation %s", Module, hk == MH_OK ? L"ok" : L"failed");
				if (hk != MH_OK)
					core::LOG(core::log_inf, L"[WC_HOOK]   reason: %S", MH_StatusToString(hk));

				hk = MH_CreateHookApi(Module, "D3DKMTSubmitCommand", (LPVOID)&Hook_D3DKMTSubmitCommand, (LPVOID*)&sRealD3DKMTSubmitCommand);
				core::LOG(core::log_inf, L"[WC_HOOK] hook %s!D3DKMTSubmitCommand %s", Module, hk == MH_OK ? L"ok" : L"failed");
				if (hk != MH_OK)
					core::LOG(core::log_inf, L"[WC_HOOK]   reason: %S", MH_StatusToString(hk));
			};

			// Try both; whichever is present will succeed.
			TryHookD3dkmtIn(L"gdi32.dll");
			TryHookD3dkmtIn(L"win32u.dll");

			// Enable just-created hooks (MH_ALL_HOOKS already enabled above for earlier ones).
			MH_STATUS hk = MH_EnableHook(MH_ALL_HOOKS);
			if (hk != MH_OK)
				core::LOG(core::log_inf, L"[WC_HOOK] MH_EnableHook(after d3dkmt) failed: %S", MH_StatusToString(hk));
		}

		core::LOG(core::log_inf, L"[WC_HOOK] enabled (MinHook ntdll NtAlloc/NtProtect/NtAllocEx/NtMapView + KernelBase/Kernel32/apiset VirtualAlloc/Protect/Alloc2 + optional gdi32 D3DKMT*=wc_hook_d3dkmt=1)");
	}
}

