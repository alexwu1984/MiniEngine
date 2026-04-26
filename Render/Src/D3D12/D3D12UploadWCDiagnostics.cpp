#include "D3D12/D3D12UploadWCDiagnostics.h"

#include "core/commandline.h"
#include "core/logger.h"

#include <windows.h>
#include <DbgHelp.h>

#include <mutex>
#include <unordered_set>

namespace RenderCore
{
	namespace
	{
		static void EnsureDbgHelpInitialized()
		{
			static std::once_flag sOnce;
			std::call_once(sOnce, []() {
				::SymSetOptions(SYMOPT_UNDNAME | SYMOPT_DEFERRED_LOADS | SYMOPT_LOAD_LINES);
				::SymInitialize(::GetCurrentProcess(), nullptr, TRUE);
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
	}

	void D3D12UploadWCDiagnostics_OnUploadMap(const wchar_t* Tag, void* MappedPtr, uint64_t SizeBytes)
	{
		if (!MappedPtr || SizeBytes == 0)
			return;
		if (!core::CommandLine::Get().GetName("d3d12_memmon"))
			return;

		MEMORY_BASIC_INFORMATION mbi = {};
		if (::VirtualQuery(MappedPtr, &mbi, sizeof(mbi)) != sizeof(mbi))
			return;
		if ((mbi.Protect & PAGE_WRITECOMBINE) == 0)
			return;

		static std::mutex sMu;
		static std::unordered_set<void*> sSeenBases;
		{
			std::lock_guard<std::mutex> lock(sMu);
			if (!sSeenBases.insert(mbi.AllocationBase).second)
				return;
		}

		static thread_local bool sReentryGuard = false;
		if (sReentryGuard)
			return;
		sReentryGuard = true;

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

		const double MB = 1024.0 * 1024.0;
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
}

