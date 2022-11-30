#pragma once

#include "core/inc.h"

namespace win32
{
    enum class dll_load_flag
    {
        none = 0,
        altered_search_path = 0x0001,
        search_user_dirs = 0x0002,
    };
    typedef core::bitflag<dll_load_flag> dll_load_flags;

    class dll
    {
    public:
        dll();
        dll(std::string path);
        ~dll();

        core::error_e load(std::string path, dll_load_flags flags = nullptr);
        void release();
        operator bool() const { return !!_handle; }

        void * proc(std::string name);

        template<typename T>
        T get(std::string name)
        {
            void * ptr = proc(name);
            return static_cast<T>(ptr);
        }

        void * handle() const { return _handle; }

    protected:
        void * _handle = nullptr;
        std::string _path;
    };
}

