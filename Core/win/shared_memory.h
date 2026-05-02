#pragma once

#include "core/inc.h"

namespace win32
{
    enum class shared_mode
    {
        none = 0,
        read = 0x1,
        write = 0x2,
        readwrite = read | write,
    };
    typedef core::bitflag<shared_mode> shared_modes;

    std::tuple<core::error_e, std::shared_ptr<void>> shared_memory_create(std::string name, size_t size, shared_modes modes);
    std::tuple<core::error_e, std::shared_ptr<void>> shared_memory_open(std::string name, shared_modes modes);
    std::tuple<core::error_e, std::shared_ptr<void>> shared_memory_access(std::string name, size_t size, shared_modes modes);

    template<typename T>
    class shared_memory
    {
    public:
        shared_memory() {}
        shared_memory(std::string name, size_t raw_size, shared_modes modes) { create(name, raw_size, modes); }
        shared_memory(std::string name, shared_modes modes) { open(name, modes); }
        shared_memory(shared_memory && another) noexcept :_ptr(std::move(another._ptr)) {}
        ~shared_memory() {}

        core::error_e create(std::string name, size_t raw_size, shared_modes modes)
        {
            auto ret = shared_memory_create(name, sizeof(T) + raw_size, modes);
            if (std::get<0>(ret))
                return std::get<0>(ret);
            _ptr = std::reinterpret_pointer_cast<T>(std::get<1>(ret));
            _name = name;
            return core::error_ok;
        }

        core::error_e open(std::string name, shared_modes modes)
        {
            auto ret = shared_memory_open(name, modes);
            if (std::get<0>(ret))
                return std::get<0>(ret);
            _ptr = std::reinterpret_pointer_cast<T>(std::get<1>(ret));
            _name = name;
            return core::error_ok;
        }

        core::error_e access(std::string name, size_t raw_size, shared_modes modes)
        {
            auto ret = shared_memory_access(name, sizeof(T) + raw_size, modes);
            if (std::get<0>(ret))
                return std::get<0>(ret);
            _ptr = std::reinterpret_pointer_cast<T>(std::get<1>(ret));
            _name = name;
            return core::error_ok;
        }

        void reset()
        {
            _ptr.reset();
        }

        core::error_e close()
        {
            _ptr.reset();
            _name.clear();
            return core::error_ok;
        }

        bool valid() const { return !!_ptr; }

        const T & operator * () const { return *_ptr; }
        T & operator * () { return *_ptr;}

        const T * operator ->() const { return _ptr.get(); }
        T * operator ->() { return _ptr.get(); }

        const T * data() const { return _ptr.get(); }
        T * data() { return _ptr.get(); }

        byte_t * raw_data() { return reinterpret_cast<byte_t *>(_ptr.get() + 1); }
        const byte_t * raw_data() const { return reinterpret_cast<const byte_t *>(_ptr.get() + 1); }

        operator bool() const { return !!_ptr; }
    private:
        std::shared_ptr<T> _ptr;
        std::string _name;
    };
}