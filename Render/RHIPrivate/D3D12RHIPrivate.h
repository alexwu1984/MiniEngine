#pragma once
#include "win/win32.h"
#include "d3dx12.h"
#include <dxgi1_4.h>
#include <delayimp.h>

namespace RenderCore
{
	/** This function is used as a SEH filter to catch only delay load exceptions. */
	inline bool IsDelayLoadException(PEXCEPTION_POINTERS ExceptionPointers)
	{
#if WINVER > 0x502	// Windows SDK 7.1 doesn't define VcppException
		switch (ExceptionPointers->ExceptionRecord->ExceptionCode)
		{
		case VcppException(ERROR_SEVERITY_ERROR, ERROR_MOD_NOT_FOUND):
		case VcppException(ERROR_SEVERITY_ERROR, ERROR_PROC_NOT_FOUND):
			return EXCEPTION_EXECUTE_HANDLER;
		default:
			return EXCEPTION_CONTINUE_SEARCH;
		}
#else
		return EXCEPTION_EXECUTE_HANDLER;
#endif
	}

	/**
* Since CreateDXGIFactory is a delay loaded import from the DXGI DLL, if the user
* doesn't have Vista/DX10, calling CreateDXGIFactory will throw an exception.
* We use SEH to detect that case and fail gracefully.
*/
	inline void SafeCreateDXGIFactory(IDXGIFactory4** DXGIFactory)
	{
		__try
		{


			CreateDXGIFactory(__uuidof(IDXGIFactory4), (void**)DXGIFactory);
		}
		__except (IsDelayLoadException(GetExceptionInformation()))
		{
			// We suppress warning C6322: Empty _except block. Appropriate checks are made upon returning. 
		}
	}
}