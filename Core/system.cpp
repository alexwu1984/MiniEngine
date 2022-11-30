#include "core/system.h"
#include "win/win32.h"
#include "core/path.h"
#include <shlobj.h>
#include <psapi.h>
#include <DbgHelp.h>

#pragma comment(lib,"DbgHelp.lib")

namespace core
{
    core::filesystem::path temp_path()
    {
        wchar_t szPath[MAX_PATH] = {};
        DWORD dwLength = ::GetTempPathW(MAX_PATH, szPath);
        return core::filesystem::path(core::ucs2_u8(std::wstring(szPath, dwLength)));
    }

    core::filesystem::path current_path()
    {
        wchar_t szPath[1024] = {};
        DWORD dwLength = ::GetCurrentDirectoryW(MAX_PATH, szPath);
        return core::ucs2_u8(std::wstring(szPath, dwLength));
    }

    bool current_path(core::filesystem::path path)
    {
        return !!::SetCurrentDirectoryW((LPCWSTR)path.u16string().c_str());
    }

    core::filesystem::path appdata_path()
    {
        wchar_t szPath[MAX_PATH] = {};
        BOOL bSucc = ::SHGetSpecialFolderPathW(NULL, szPath, CSIDL_APPDATA, false);
        if (!bSucc)
            return temp_path();
        return core::filesystem::path(core::ucs2_u8(std::wstring(szPath, std::wcslen(szPath))));
    }

	std::wstring getwide_appdata_path()
	{
		wchar_t szPath[MAX_PATH] = {};
		BOOL bSucc = ::SHGetSpecialFolderPathW(NULL, szPath, CSIDL_APPDATA, false);
		if (!bSucc)
			return temp_path();
		std::wstring strPath = std::wstring(szPath);

		int nReturn = SHCreateDirectoryEx(NULL, strPath.c_str(), NULL);
		(void)nReturn;

		return strPath;
	}

	core::filesystem::path process_path()
    {
        wchar_t temp[512] = {};
        int32_t nchars = GetModuleFileNameW(NULL, temp, 512);
        return core::filesystem::path(std::wstring(temp, nchars));
    }

    core::filesystem::path process_directory()
    {
        wchar_t temp[512] = {};
        int32_t nchars = GetModuleFileNameW(NULL, temp, 512);
        return core::filesystem::path(std::wstring(temp, nchars)).parent_path();
    }

    core::filesystem::path module_directory()
    {
        HMODULE hModule = NULL;
        ::GetModuleHandleEx(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS, (LPCTSTR)module_directory, &hModule);
        if (!hModule)
            return {};

        wchar_t temp[512] = {};
        int32_t nchars = GetModuleFileNameW(hModule, temp, 512);
        return core::filesystem::path(std::wstring(temp, nchars)).parent_path();
    }

	bool create_directory(const std::wstring& path)
	{
		return SHCreateDirectoryEx(nullptr, path.c_str(), nullptr) != ERROR_SUCCESS;
	}

    std::string process_name()
    {
        return process_path().filename().string();
    }

    uint32_t thread_id()
    {
        return ::GetCurrentThreadId();
    }
    uint32_t process_id()
    {
        return ::GetCurrentProcessId();
    }

	uint32_t g_gamethread = 0;

	void set_game_thread_id(uint32_t id)
	{
		g_gamethread = id;
	}

	bool is_in_gamethread()
	{
		return g_gamethread == thread_id();
	}

	bool isFileExist(filesystem::path path)
    {
        return !_waccess(path.generic_wstring().c_str(), 0);
    }

    static void __thread_set_name(int thread_id, const char * name)
    {
        if (!thread_id)
            thread_id = GetCurrentThreadId();

#pragma pack(push, 8)
        struct THREADNAME_INFO
        {
            uint32_t dwType; // must be 0x1000
            const char * name; // pointer to name (in same addr space)
            int thread_id; // thread ID (-1 caller thread)
            uint32_t flags; // reserved for future use, most be zero
        };
#pragma pack(pop)

        const uint32_t MS_VC_EXCEPTION_SET_THREAD_NAME = 0x406d1388;
        THREADNAME_INFO info;
        info.dwType = 0x1000;
        info.name = name;
        info.thread_id = thread_id;
        info.flags = 0;
        __try
        {
            RaiseException(MS_VC_EXCEPTION_SET_THREAD_NAME, 0, sizeof(THREADNAME_INFO) / sizeof(ULONG_PTR), (ULONG_PTR *)&info);
        }
        __except (EXCEPTION_CONTINUE_EXECUTION)
        {
        }
    }

    void thread_set_name(int thread_id, const char * name)
    {
        if (!thread_id)
            thread_id = GetCurrentThreadId();

        __thread_set_name(thread_id, name);
    }

    void thread_set_priority(thread_priority priority)
    {
        switch (priority)
        {
        case thread_priority_idle: ::SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_IDLE); break;
        case thread_priority_lowest: ::SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_LOWEST); break;
        case thread_priority_low: ::SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_BELOW_NORMAL); break;
        case thread_priority_normal: ::SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_NORMAL); break;
        case thread_priority_high: ::SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_ABOVE_NORMAL); break;
        case thread_priority_highest: ::SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_HIGHEST); break;
        case thread_priority_realtime: ::SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_TIME_CRITICAL); break;
        default: break;
        }

    }

	bool save_as_bitmap(const char* pData, uint32_t nLen, int w, int h, int bitCount, const wchar_t* pszFileName)
	{
		// Writes a BMP file
		// save to file
		HANDLE hFile = ::CreateFile(pszFileName,            // file to create 
			GENERIC_WRITE,                // open for writing 
			0,                            // do not share 
			NULL,                         // default security 
			OPEN_ALWAYS,                  // overwrite existing 
			FILE_ATTRIBUTE_NORMAL,        // normal file 
			NULL);                        // no attr. template 
		if (!hFile || hFile == INVALID_HANDLE_VALUE)
		{
			return false;	// 
		}

		DWORD dwSizeBytes = nLen;

		// fill in the headers
		BITMAPFILEHEADER bmfh;
		bmfh.bfType = 0x4D42; // 'BM'
		bmfh.bfSize = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER) + dwSizeBytes;//整个文件的大小
		bmfh.bfReserved1 = 0;
		bmfh.bfReserved2 = 0;
		bmfh.bfOffBits = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER);//图像数据偏移量，即图像数据在文件中的保存位置

		DWORD dwBytesWritten;
		::WriteFile(hFile, &bmfh, sizeof(bmfh), &dwBytesWritten, NULL);
		if (dwBytesWritten != sizeof(bmfh))
		{
		}

		BITMAPINFOHEADER bmih;

		bmih.biSize = sizeof(BITMAPINFOHEADER);
		bmih.biWidth = w;
		bmih.biHeight = -h;
		bmih.biPlanes = 1; // 图像的目标显示设备的位数，通常为1
		bmih.biBitCount = bitCount; // 每个像素的位数，可以为1、4、8、24、32
		bmih.biCompression = BI_RGB;// 是否压缩
		bmih.biSizeImage = 0;//图像大小的字节数
		bmih.biXPelsPerMeter = 0;
		bmih.biYPelsPerMeter = 0;
		bmih.biClrUsed = 0;
		bmih.biClrImportant = 0;

		::WriteFile(hFile, &bmih, sizeof(bmih), &dwBytesWritten, NULL);
		if (dwBytesWritten != sizeof(bmih))
		{
		}

		::WriteFile(hFile, pData, dwSizeBytes, &dwBytesWritten, NULL);
		if (dwBytesWritten != dwSizeBytes)
		{
		}

		::CloseHandle(hFile);
		return true;
	}

	bool create_process(const std::wstring& cmdLine)
	{
		PROCESS_INFORMATION pi = { 0 };
		wchar_t* cmd_line_w = NULL;
		STARTUPINFOW si = { 0 };
		si.cb = sizeof(si);
		bool success = false;

		if (cmdLine.length()) {
			success = !!CreateProcessW(NULL, (LPWSTR)cmdLine.c_str(), NULL, NULL, true,
				CREATE_NO_WINDOW, NULL, NULL, &si, &pi);

			if (success) {
				WaitForSingleObject(pi.hProcess, 2 * 1000);
				CloseHandle(pi.hProcess);
				CloseHandle(pi.hThread);
			}
		}
		return success;
	}

	bool ceate_dump(const wchar_t* name_prefix, struct _EXCEPTION_POINTERS* ExceptionInfo)
	{
		SYSTEMTIME time;
		GetLocalTime(&time);
		wchar_t tmpfilename[MAX_PATH];
		_snwprintf_s(tmpfilename, MAX_PATH, L"%s%04d%02d%02d%02d%02d%02d%03d.dmp", name_prefix, time.wYear, time.wMonth, time.wDay, time.wHour, time.wMinute, time.wSecond, time.wMilliseconds);

		std::wstring dmppath = tmpfilename;
		HANDLE hFile = CreateFileW(dmppath.c_str(), GENERIC_WRITE | GENERIC_READ, FILE_SHARE_WRITE | FILE_SHARE_READ, NULL, CREATE_ALWAYS,
			FILE_ATTRIBUTE_NORMAL, NULL);

		_MINIDUMP_EXCEPTION_INFORMATION ExInfo;

		ExInfo.ThreadId = ::GetCurrentThreadId();
		ExInfo.ExceptionPointers = ExceptionInfo;
		ExInfo.ClientPointers = NULL;

#ifndef _DEBUG
		BOOL bOK = MiniDumpWriteDump(GetCurrentProcess(), GetCurrentProcessId(), hFile, MiniDumpWithFullMemoryInfo, &ExInfo, NULL, NULL);
#else
		BOOL bOK = MiniDumpWriteDump(GetCurrentProcess(), GetCurrentProcessId(), hFile, MiniDumpNormal, &ExInfo, NULL, NULL);
#endif

		CloseHandle(hFile);
		return true;
	}
 
	#define LOG_BUFFER_SIZE 65536	 
	static wchar_t s_sLogBuffer[LOG_BUFFER_SIZE];

	void outputDebugString(const wchar_t* pcString, ...)
	{
		char* pArgs;
		pArgs = (char*)&pcString + sizeof(pcString);
		_vstprintf_s(s_sLogBuffer, LOG_BUFFER_SIZE, pcString, pArgs);
		OutputDebugString(s_sLogBuffer);
	}

}
#ifdef WINDOWS_PLATFORM_TYPES_GUARD
#include "Windows/HideWindowsPlatformTypes.h"
#endif