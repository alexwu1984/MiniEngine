#include "win/win32.h"
#include "dxgicommon.h"
#include <d3d11.h>

void DXGI::CopyGraphicDXGI(const byte_t* srcBits, byte_t *targetBuffer, int srcPitch, int targetPitch, int format, core::vec2i size)
{
	switch (format)
	{
	case DXGI_FORMAT_B8G8R8A8_TYPELESS:
	case DXGI_FORMAT_R8G8B8A8_UNORM:
	case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB:
		//		LOGINFO(L"GetFrameFullSize: source format = DXGI_FORMAT_R8G8B8A8_UNORM or DXGI_FORMAT_R8G8B8A8_UNORM_SRGB");
		// 32-bit entries: discard alpha
		// swap R and B channels (RGBA to BGRA) DX9 used little endianness	
	{
		BYTE *pSrcBegin = const_cast<BYTE *>(srcBits);
		BYTE *pDest = targetBuffer;

		for (int height = 0; height < size.cy; height++)
		{
			BYTE *pSrcCopy = pSrcBegin;
			BYTE *pDestWrite = pDest;
			for (int width = 0; width < size.cx; ++width)
			{
				BYTE b = *pSrcCopy++;
				BYTE g = *pSrcCopy++;
				BYTE r = *pSrcCopy++;

				*pDestWrite++ = r;
				*pDestWrite++ = g;
				*pDestWrite++ = b;
				*pDestWrite++ = *pSrcCopy++;
			}
			pSrcBegin = pSrcBegin + srcPitch;
			pDest = pDest + targetPitch;
		}
	}
	break;
	case DXGI_FORMAT_B8G8R8A8_UNORM:
	{
		LPBYTE src = (LPBYTE)srcBits;
		LPBYTE dst = (LPBYTE)targetBuffer;
		for (int i = 0; i < size.cy; i++)
		{
			memcpy(dst, src, targetPitch);
			src += srcPitch;
			dst += targetPitch;
		}
	}
	break;

	case DXGI_FORMAT_R10G10B10A2_UNORM:
		for (int i = 0, k = 0; i < size.cy && k < size.cy; i++, k++)
		{
			for (int j = 0; j < size.cx; j++)
			{
				WORD w1 = MAKEWORD(srcBits[k*srcPitch + j * 4], srcBits[k*srcPitch + j * 4 + 1]);
				WORD w2 = MAKEWORD(srcBits[k*srcPitch + j * 4 + 2], srcBits[k*srcPitch + j * 4 + 3]);
				DWORD dw = MAKELONG(w1, w2);

				UINT b = (dw & 0x000003ff);
				UINT g = ((dw >> 10) & 0x000003ff);
				UINT r = ((dw >> 20) & 0x000003ff);

				targetBuffer[i*targetPitch + j * 4] = (BYTE)(r >> 2);
				targetBuffer[i*targetPitch + j * 4 + 1] = (BYTE)(g >> 2);
				targetBuffer[i*targetPitch + j * 4 + 2] = (BYTE)(b >> 2);
				targetBuffer[i*targetPitch + j * 4 + 3] = 255;
			}
		}
		break;
	case DXGI_FORMAT_B8G8R8X8_UNORM:
	{
		BYTE *pSrcStart = const_cast<BYTE *>(srcBits);
		BYTE *pDest = targetBuffer;

		for (int i = 0; i < size.cy; i++)
		{
			BYTE *pSrcCopy = pSrcStart;
			BYTE *pDestWrite = pDest;
			for (int width = 0; width < size.cx; ++width)
			{
				*pDestWrite++ = *pSrcCopy++;
				*pDestWrite++ = *pSrcCopy++;
				*pDestWrite++ = *pSrcCopy++;
				*pDestWrite++ = 255;
				++pSrcCopy;
			}
			pSrcStart = pSrcStart + srcPitch;
			pDest = pDest + targetPitch;
		}
	}
	break;
	}
}

DXGI_FORMAT DXGI::FixCopyTextureFormat(DXGI_FORMAT format)
{
	switch (format)
	{
	case DXGI_FORMAT_B8G8R8A8_UNORM_SRGB: return DXGI_FORMAT_B8G8R8A8_UNORM;
	case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB: return DXGI_FORMAT_R8G8B8A8_UNORM;
	}

	return format;
}

HMODULE DXGI::GetSystemModule(const wchar_t *module)
{
	wchar_t system_path[MAX_PATH] = { 0 };
	GetSystemDirectoryW(system_path, MAX_PATH);
	wchar_t base_path[MAX_PATH];

	wcscpy_s(base_path, system_path);
	wcscat_s(base_path, L"\\");
	wcscat_s(base_path, module);
	return GetModuleHandleW(base_path);
}

std::wstring DXGI::GetSystemDLLPath()
{
	wchar_t system_path[MAX_PATH] = { 0 };
	GetSystemDirectoryW(system_path, MAX_PATH);
	return system_path;
}
