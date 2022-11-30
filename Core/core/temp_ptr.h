#pragma once

namespace core
{
    template<typename _Ptr, typename _Dt>
    class temp_ptr
    {
    public:
        temp_ptr() {}
        temp_ptr(std::nullptr_t)
        {
        }

        temp_ptr(const temp_ptr<_Ptr, _Dt> & another)
        {
            if (_ptr != another._ptr)
            {
                _ptr = another._ptr;
                _deleter = another._deleter;
            }
        }

        temp_ptr(temp_ptr<_Ptr, _Dt> && another) noexcept
        {
            _ptr = another._ptr;
            _deleter = another._deleter;
            another._ptr = nullptr;
            another._deleter = nullptr;
        }

        explicit temp_ptr(_Ptr * ptr) :_ptr(ptr) {}
        temp_ptr(_Ptr * ptr, _Dt _deleter) :_ptr(ptr), _deleter(_deleter) {}
        ~temp_ptr()
        {
            if (_ptr && _deleter)
                _deleter(_ptr);
        }

        _Ptr ** operator & ()
        {
            return &_ptr;
        }

        _Ptr * operator ->() const
        {
            return _ptr;
        }

        operator bool() const { return !!_ptr; }

        temp_ptr<_Ptr, _Dt> & operator=(const temp_ptr<_Ptr, _Dt> & another)
        {
            if (_ptr && _deleter)
                _deleter(_ptr);


            _ptr = another._ptr;
            _deleter = another._deleter;
            return *this;
        }

        void reset()
        {
            if (_ptr && _deleter)
                _deleter(_ptr);

            _ptr = nullptr;
            _deleter = nullptr;
        }

        void reset(_Ptr * ptr, const _Dt & deleter)
        {
            if (_ptr && _deleter)
                _deleter(_ptr);

            _ptr = ptr;
            _deleter = deleter;
        }

        _Ptr * get() const { return _ptr; }
        _Ptr ** getpp() const { return const_cast<_Ptr **>(&_ptr); }
        void ** getvv() const { return static_cast<void **>(&_ptr); }

        _Ptr * release()
        {
            _Ptr * ptr = _ptr;
            _ptr = nullptr;
            _deleter = nullptr;
            return ptr;
        }

        bool operator == (const temp_ptr<_Ptr, _Dt> & another) const
        {
            return _ptr == another._ptr;
        }

        bool operator != (const temp_ptr<_Ptr, _Dt> & another) const
        {
            return _ptr != another._ptr;
        }

    protected:
        _Ptr * _ptr = nullptr;
        _Dt _deleter = nullptr;
    };
}
