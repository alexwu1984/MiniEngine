#pragma once
#include "core/inc.h"
#include "core/vec2.h"
#include <dxgi.h>

namespace DXGI
{
	void CopyGraphicDXGI(const byte_t* srcBits, byte_t *targetBuffer, int srcPitch, int targetPitch, int format, core::vec2i size);
	DXGI_FORMAT FixCopyTextureFormat(DXGI_FORMAT format);
	HMODULE GetSystemModule(const wchar_t *module);
	std::wstring GetSystemDLLPath();
}