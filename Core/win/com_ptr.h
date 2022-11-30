#pragma once

#include "core/inc.h"

namespace win32
{
    template<typename UT>
    UT * com_ref(UT * ptr)
    {
        if (!ptr)
            return ptr;
        ptr->AddRef();
        return ptr;
    }

    template<typename UT>
    void com_release(UT * ptr)
    {
        if (!ptr)
            return;
        ptr->Release();
    }

    template<typename UT>
    class com_ptr
    {
    public:
        com_ptr() {}
        com_ptr(std::nullptr_t)
        {
        }

        com_ptr(const com_ptr<UT> & another)
        {
            if (another._ptr)
            {
                _ptr = another._ptr;
                if (_ptr)
                    _ptr->AddRef();
            }
        }

        template<typename T>
        com_ptr(const com_ptr<T> & another)
        {
            if ((void *)_ptr != (void *)another.get())
            {
                if (_ptr)
                    _ptr->Release();

                if (another.get())
                    another.get()->QueryInterface(__uuidof(UT), (void **)&_ptr);
            }
        }

        explicit com_ptr(UT * ptr) :_ptr(ptr)
        {
            
        }
        ~com_ptr()
        {
            if (_ptr)
            {
                _ptr->Release();
                _ptr = nullptr;
            }
        }

        UT ** operator & ()
        {
            return &_ptr;
        }

        UT * operator ->() const
        {
            if (!_ptr)
                throw std::exception("null com ptr");
            return _ptr;
        }

        operator bool() const { return !!_ptr; }

        com_ptr<UT> & operator=(const com_ptr<UT> & another)
        {
            if (_ptr == another._ptr)
                return *this;

            if (_ptr)
            {
                _ptr->Release();
                _ptr = nullptr;
            }

            if (another._ptr)
            {
                another._ptr->AddRef();
                _ptr = another._ptr;
            }
            return *this;
        }

        void reset()
        {
            if (_ptr)
            {
                _ptr->Release();
                _ptr = nullptr;
            }
        }

        void reset(UT * ptr)
        {
            if (_ptr == ptr)
                return;

            if (_ptr)
            {
                _ptr->Release();
                _ptr = nullptr;
            }
            if (ptr)
                ptr->AddRef();
            _ptr = ptr;
        }

        UT * get() const { return _ptr; }
        UT ** getpp() const { if (_ptr) throw 1;  return const_cast<UT **>(&_ptr); }
        void ** getvv() const { if (_ptr) throw 1;  return (void **)(&_ptr); }
        template<typename UT2>
        UT2 ** gettt() const { if (_ptr) throw 1;  return reinterpret_cast<UT2 **>(getvv()); }

        UT * ref() const
        {
            if (_ptr)
                _ptr->AddRef();
            return _ptr;
        }

        bool operator == (const com_ptr<UT> & another) const
        {
            return _ptr == another._ptr;
        }

        bool operator != (const com_ptr<UT> & another) const
        {
            return _ptr != another._ptr;
        }

        std::shared_ptr<UT> release_to_shared()
        {
            auto ptr = std::shared_ptr<UT>(_ptr, com_release<UT>);
            _ptr = nullptr;
            return ptr;
        }
    protected:
        UT * _ptr = nullptr;
    };
}
