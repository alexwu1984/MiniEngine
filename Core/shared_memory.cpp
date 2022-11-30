#include "win/shared_memory.h"
#include "win/win32.h"
#include "core/logger.h"

namespace win32
{
    std::tuple<core::error_e, std::shared_ptr<void>> shared_memory_create(std::string name, size_t size, shared_modes modes)
    {
        std::wstring wname = core::u8_ucs2(name);
        LARGE_INTEGER li = {};
        li.QuadPart = size;
        HANDLE handle = CreateFileMappingW(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, li.HighPart, li.LowPart, wname.c_str());
        if (!handle)
            return { winerr(), nullptr };

        if(GetLastError() == ERROR_ALREADY_EXISTS)
        {
            CloseHandle(handle);
            return { core::error_exists, nullptr };
        }

        DWORD dwViewAccess = 0;
        if (modes & shared_mode::read)
            dwViewAccess |= FILE_MAP_READ;
        if (modes & shared_mode::write)
            dwViewAccess |= FILE_MAP_WRITE;

        void * data_ = MapViewOfFile(handle, dwViewAccess, 0, 0, 0);
        if (!data_)
        {
            CloseHandle(handle);
            return { winerr(), nullptr };
        }

        auto data = std::shared_ptr<void>(data_, [handle](void * ptr) { if (ptr) UnmapViewOfFile(ptr); if (handle) CloseHandle(handle); });
        return { core::error_ok, data };
    }

    std::tuple<core::error_e, std::shared_ptr<void>> shared_memory_open(std::string name, shared_modes modes)
    {
        std::wstring wname = core::u8_ucs2(name);
        DWORD dwMapAccess = 0;
        if (modes & shared_mode::read)
            dwMapAccess |= FILE_MAP_READ;
        if (modes & shared_mode::write)
            dwMapAccess |= FILE_MAP_WRITE;
        HANDLE handle = OpenFileMappingW(dwMapAccess, false, wname.c_str());
        if (!handle)
            return { winerr(), nullptr };

        DWORD dwViewAccess = 0;
        if (modes & shared_mode::read)
            dwViewAccess |= FILE_MAP_READ;
        if (modes & shared_mode::write)
            dwViewAccess |= FILE_MAP_WRITE;

        void * data_ = MapViewOfFile(handle, dwViewAccess, 0, 0, 0);
        if (!data_)
        {
            CloseHandle(handle);
            return { winerr(), nullptr };
        }

        auto data = std::shared_ptr<void>(data_, [handle](void * ptr) { if (ptr) UnmapViewOfFile(ptr); if (handle) CloseHandle(handle);  });
        return { core::error_ok, data };
    }

    std::tuple<core::error_e, std::shared_ptr<void>> shared_memory_access(std::string name, size_t size, shared_modes modes)
    {
        std::wstring wname = core::u8_ucs2(name);
        LARGE_INTEGER li = {};
        li.QuadPart = size;
        HANDLE handle = CreateFileMappingW(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, li.HighPart, li.LowPart, wname.c_str());
        if (!handle)
            return { winerr(), nullptr };

        if (GetLastError() == ERROR_ALREADY_EXISTS)
        {
            CloseHandle(handle);
            DWORD dwMapAccess = 0;
            if (modes & shared_mode::read)
                dwMapAccess |= FILE_MAP_READ;
            if (modes & shared_mode::write)
                dwMapAccess |= FILE_MAP_WRITE;
            handle = OpenFileMappingW(dwMapAccess, false, wname.c_str());
            if (!handle)
                return { winerr(), nullptr };
        }

        DWORD dwViewAccess = 0;
        if (modes & shared_mode::read)
            dwViewAccess |= FILE_MAP_READ;
        if (modes & shared_mode::write)
            dwViewAccess |= FILE_MAP_WRITE;

        void * data_ = MapViewOfFile(handle, dwViewAccess, 0, 0, 0);
        if (!data_)
        {
            CloseHandle(handle);
            return { winerr(), nullptr };
        }

        auto data = std::shared_ptr<void>(data_, [handle](void * ptr) { if (ptr) UnmapViewOfFile(ptr); if (handle) CloseHandle(handle); });
        return { core::error_ok, data };
    }
}
#ifdef WINDOWS_PLATFORM_TYPES_GUARD
#include "Windows/HideWindowsPlatformTypes.h"
#endif