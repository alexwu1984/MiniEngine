#pragma once
#include "RHI/DynamicRHI.h"
#include "core/logger.h"
#include "core/strings.h"
#include <comdef.h>
#include <exception>

namespace Engine
{
	/** Log full _com_error (HRESULT + ErrorMessage) then latch GPU fatal / WM_QUIT path used elsewhere. */
	inline void LogComErrorToEngineLog(const wchar_t* where, const _com_error& e)
	{
		const wchar_t* msg = e.ErrorMessage();
		if (!msg)
			msg = L"(null ErrorMessage)";
		core::LOG(core::log_e::log_err,
				  L"%s: _com_error HRESULT=0x%08X %s",
				  where ? where : L"(unknown)",
				  (unsigned)e.Error(),
				  msg);
		RenderCore::RHI_NotifyFatalGpuDeviceLoss(where ? where : L"_com_error", e.Error(), S_OK);
	}

	inline void LogStdExceptionToEngineLog(const wchar_t* where, const std::exception& e)
	{
		core::LOG(core::log_e::log_err,
				  L"%s: std::exception %s",
				  where ? where : L"(unknown)",
				  core::u8_ucs2(e.what()).c_str());
	}

	inline void LogUnknownExceptionToEngineLog(const wchar_t* where)
	{
		core::LOG(core::log_e::log_err, L"%s: non-_com_error / unknown C++ exception", where ? where : L"(unknown)");
	}
}
