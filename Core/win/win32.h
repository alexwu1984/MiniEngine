#pragma once

#include "core/inc.h"

#include <SDKDDKVer.h>
#if defined UE_EDITOR || defined UE_GAME || defined UE_SERVER
	#ifndef WINDOWS_PLATFORM_TYPES_GUARD
		#include <Runtime/Core/Public/Windows/AllowWindowsPlatformTypes.h>
	#endif
#else
	#define WIN32_LEAN_AND_MEAN
	#include <windows.h>
#endif

//----------------------------------------------------------------------

#include "core/strings.h"

namespace win32
{
    enum res_e
    {
        // "FILE"
        res_file = 0,
        res_count
    };

    core::error_e winerr();
    core::error_e winerr_val(uint32_t err);
    std::string winerr_str();
    std::string winerr_str(uint32_t err);
    int runLoop();

    std::string GUID2String(const GUID & guid);
    std::string GUID2String(std::array<uint8_t, 16> data);

    std::tuple<std::shared_ptr<byte_t>, int32_t> load_res(uint32_t id, res_e type = res_file);
    std::tuple<std::shared_ptr<byte_t>, int32_t> load_res(const char * res, res_e type = res_file);

    struct winversion_t
    {
        uint32_t major;
        uint32_t minor;
        uint32_t pack;
        uint32_t build;

        int32_t diff(const winversion_t & another) const
        {
            int32_t ret = 0;
            if (major > another.major) ret += 1000;
            else if (major < another.major) ret -= 1000;
            else {}
            if (minor > another.minor) ret += 100;
            else if (minor < another.minor) ret -= 100;
            else {}
            if (pack > another.pack) ret += 10;
            else if (pack < another.pack) ret -= 10;
            else {}
            if (build > another.build) ret += 1;
            else if (build < another.build) ret -= 1;
            else {}
            return ret;
        }
        bool operator > (const winversion_t & another) const { return diff(another) > 0; }
        bool operator < (const winversion_t & another) const { return diff(another) < 0; }
        bool operator >= (const winversion_t & another) const{ return diff(another) >= 0; }
        bool operator <= (const winversion_t & another) const{ return diff(another) <= 0; }
        bool operator == (const winversion_t & another) const{ return diff(another) == 0; }
        bool operator != (const winversion_t & another) const{ return diff(another) != 0; }
    };

    constexpr winversion_t winversion_10 = { 10, 0, 0, 0 };
    constexpr winversion_t winversion_8_1 = { 6, 3, 0, 0 };
    constexpr winversion_t winversion_8 = { 6, 2, 0, 0 };
    constexpr winversion_t winversion_7 = { 6, 1, 0, 0 };
    constexpr winversion_t winversion_vista = { 6, 0, 0, 0 };
    constexpr winversion_t winversion_server_2003 = { 5, 2, 0, 0 };
    constexpr winversion_t winversion_xp_sp3 = { 5, 1, 3, 0 };
    constexpr winversion_t winversion_xp_sp2 = { 5, 1, 2, 0 };
    constexpr winversion_t winversion_xp_sp1 = { 5, 1, 1, 0 };
    constexpr winversion_t winversion_xp = { 5, 1, 0, 0 };
    constexpr winversion_t winversion_2000 = { 5, 0, 0, 0 };
    winversion_t version();
    std::string version_str(const winversion_t & ver);
    inline std::ostream & operator << (std::ostream & ost, const winversion_t & ver)
    {
        return ost << "Windows " << version_str(ver) << " [" << ver.major << "." << ver.minor << "." << ver.pack << "." << ver.build << "]";
    }
    inline std::wostream & operator << (std::wostream & ost, const winversion_t & ver)
    {
        return ost << L"Windows " << core::u8_ucs2(version_str(ver)) << L" [" << ver.major << L"." << ver.minor << L"." << ver.pack << L"." << ver.build << L"]";
    }
}
