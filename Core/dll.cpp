#include "win/dll.h"
#include "win/win32.h"
#include "core/logger.h"

namespace win32
{
    using namespace core;

    dll::dll() :
        _handle(NULL)
    {

    }

    dll::dll(std::string path) :
        _handle(NULL)
    {
        load(path);
    }

    dll::~dll()
    {
        release();
    }

    core::error_e dll::load(std::string path, dll_load_flags flags)
    {
        release();
        if (!path.empty())
        {
            core::bitflag<uint32_t> laodflags;
            laodflags.set(LOAD_LIBRARY_SEARCH_USER_DIRS, flags.any(dll_load_flag::search_user_dirs));
            laodflags.set(LOAD_WITH_ALTERED_SEARCH_PATH, flags.any(dll_load_flag::altered_search_path));
            if (flags.any(dll_load_flag::altered_search_path) && (path.size() < 2 || path[1] != ':'))
                core::war() << __FUNCTION__" LoadLibrary need to specifies an absolute path with LOAD_WITH_ALTERED_SEARCH_PATH flag!";

            std::wstring pathw = core::u8_ucs2(path);
            _handle = ::LoadLibraryExW(pathw.c_str(), NULL, laodflags);
            if (!_handle)
                logger::war() << __FUNCTION__" LoadLibrary [" << pathw << "] failed " << winerr_str(GetLastError());
            else
            {
                wchar_t temp[1024] = {};
                uint32_t len =GetModuleFileNameW((HMODULE)_handle, temp, 1024);
                _path = core::ucs2_u8(temp, len);
            }
        }
        return _handle ? error_ok : error_inner;
    }

    void dll::release()
    {
        if (_handle)
        {
            ::FreeLibrary((HMODULE)_handle);
            _handle = NULL;
        }
    }

    void * dll::proc(std::string name)
    {
        if (_handle)
        {
            std::string namea = core::u8_ansi(name);
            return (void *)::GetProcAddress((HMODULE)_handle, namea.c_str());
        }
        else
            return nullptr;
    }
}
