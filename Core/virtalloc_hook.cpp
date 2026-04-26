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

		// Aggregate to avoid log spam when commit/protect happens in small chunks.
		static volatile LONG64 sWcCommitBytes = 0;
		static volatile LONG64 sWcProtectBytes = 0;

		static void LogStackOncePerSecond(const wchar_t* Tag, void* Base, SIZE_T Size, DWORD Protect)
		{
			static thread_local bool sReentryGuard = false;
			if (sReentryGuard)
				return;
			sReentryGuard = true;

			static ULONGLONG sLastTick = 0;
			const ULONGLONG now = ::GetTickCount64();
			if (now - sLastTick < 1000)
			{
				sReentryGuard = false;
				return;
			}
			sLastTick = now;

			const LONG64 wcCommit = ::InterlockedExchange64(&sWcCommitBytes, 0);
			const LONG64 wcProtect = ::InterlockedExchange64(&sWcProtectBytes, 0);

			void* frames[32] = {};
			USHORT n = ::RtlCaptureStackBackTrace(1, 32, frames, nullptr);
			core::LOG(core::log_inf,
					  L"[WC_HOOK] %s base=%p size=%.1fMB protect=0x%08x frames=%u | agg(WC Commit=%.1fMB Protect=%.1fMB)/s",
					  Tag, Base, (double)Size / (1024.0 * 1024.0), (unsigned)Protect, (unsigned)n,
					  (double)wcCommit / (1024.0 * 1024.0), (double)wcProtect / (1024.0 * 1024.0));

			// Symbolize best-effort (can be noisy; keep it short).
			HANDLE proc = ::GetCurrentProcess();
			static bool sSymInited = false;
			if (!sSymInited)
			{
				::SymInitialize(proc, nullptr, TRUE);
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

			sReentryGuard = false;
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

		st = MH_EnableHook(MH_ALL_HOOKS);
		if (st != MH_OK)
		{
			core::LOG(core::log_inf, L"[WC_HOOK] MH_EnableHook failed: %S", MH_StatusToString(st));
			return;
		}

		core::LOG(core::log_inf, L"[WC_HOOK] enabled (MinHook ntdll NtAlloc/NtProtect/NtAllocEx/NtMapView)");
	}
}

