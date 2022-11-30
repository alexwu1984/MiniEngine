#pragma once

namespace core
{
    template <class T, std::size_t = sizeof(T)>
    std::true_type is_complete_impl(T *);

    std::false_type is_complete_impl(...);

    template <class T>
    using is_complete = decltype(is_complete_impl(std::declval<T*>()));

    template <class T>
    constexpr bool is_complete_v = is_complete<T>::value;

    template <typename T, typename = void>
    struct bitflag_helper
    {
        typedef T underlying_type;
    };

    template <typename T>
    struct bitflag_helper<T, std::enable_if_t<std::is_enum_v<T>>>
    {
        typedef typename std::underlying_type<T>::type underlying_type;
    };

    template<typename Enum>
    struct bitflag
    {
        bitflag() = default;
        bitflag(Enum e) : _e(e) {}
        bitflag(std::nullptr_t) : _e(static_cast<Enum>(0)) {}

        using underlying = typename bitflag_helper<Enum>::underlying_type;

        bitflag & operator = (Enum e)
        {
            _e = e;
            return *this;
        }

        bitflag & operator = (const bitflag & another)
        {
            _e = another._e;
            return *this;
        }

        bitflag & set(Enum e, bool b)
        {
            if(b)
                _e = static_cast<Enum>(static_cast<underlying>(_e) | static_cast<underlying>(e));
            else
                _e = static_cast<Enum>(static_cast<underlying>(_e) & (~static_cast<underlying>(e)));
            return *this;
        }

        bitflag & clear()
        {
            _e = static_cast<Enum>(0);
            return *this;
        }

        bitflag & clear(Enum e)
        {
            _e = static_cast<Enum>(static_cast<underlying>(_e) & (~static_cast<underlying>(e)));
            return *this;
        }

        bool any() const
        {
            return static_cast<underlying>(_e) != 0;
        }

        bool any(Enum e) const
        {
            return (static_cast<underlying>(_e) & static_cast<underlying>(e)) != 0;
        }

        bool all(Enum e) const
        {
            return (static_cast<underlying>(_e) & static_cast<underlying>(e)) == static_cast<underlying>(e);
        }

        bool any(bitflag bf) const
        {
            return (static_cast<underlying>(_e) & static_cast<underlying>(bf._e)) != 0;
        }

        bitflag operator | (Enum e) const
        {
            return static_cast<Enum>(static_cast<underlying>(_e) | static_cast<underlying>(e));
        }

        bitflag operator | (const bitflag & f) const
        {
            return static_cast<Enum>(static_cast<underlying>(_e) | static_cast<underlying>(f._e));
        }

        bitflag operator & (Enum e) const
        {
            return static_cast<Enum>(static_cast<underlying>(_e) & static_cast<underlying>(e));
        }

        bitflag operator & (const bitflag & f) const
        {
            return static_cast<Enum>(static_cast<underlying>(_e) & static_cast<underlying>(f._e));
        }

        bitflag operator !() const
        {
            return static_cast<Enum>(!static_cast<underlying>(_e));
        }

        bitflag operator ~() const
        {
            return static_cast<Enum>(~static_cast<underlying>(_e));
        }

        bitflag & operator |= (Enum e)
        {
            _e = static_cast<Enum>(static_cast<underlying>(_e) | static_cast<underlying>(e));
            return *this;
        }

        bitflag & operator |= (const bitflag & f)
        {
            _e = static_cast<Enum>(static_cast<underlying>(_e) | static_cast<underlying>(f._e));
            return *this;
        }

        bitflag & operator &= (Enum e)
        {
            _e = static_cast<Enum>(static_cast<underlying>(_e) & static_cast<underlying>(e));
            return *this;
        }

        bitflag & operator &= (const bitflag & f)
        {
            _e = static_cast<Enum>(static_cast<underlying>(_e) & static_cast<underlying>(f._e));
            return *this;
        }

        operator underlying() const
        {
            return static_cast<underlying>(_e);
        }

        const Enum & get() const { return _e; }
        Enum & get() { return _e; }

    private:
        Enum _e = static_cast<Enum>(0);
    };

    template<typename Enum>
    struct enable_bitmasks
    {
        static const bool enable = is_complete_v<bitflag<Enum>> && std::is_enum_v<Enum> && !std::is_convertible_v<Enum, int>;
    };
}


template<typename Enum>
constexpr typename std::enable_if<core::enable_bitmasks<Enum>::enable, Enum>::type operator |(const Enum & lhs, const Enum & rhs)
{
    using underlying = typename std::underlying_type<Enum>::type;
    return static_cast<Enum>(static_cast<underlying>(lhs) | static_cast<underlying>(rhs));
}

template<typename Enum>
constexpr typename std::enable_if<core::enable_bitmasks<Enum>::enable, Enum>::type operator &(const Enum & lhs, const Enum & rhs)
{
    using underlying = typename std::underlying_type<Enum>::type;
    return static_cast<Enum>(static_cast<underlying>(lhs) & static_cast<underlying>(rhs));
}

template<typename Enum>
constexpr typename std::enable_if<core::enable_bitmasks<Enum>::enable, Enum>::type & operator |=(Enum & lhs, const Enum & rhs)
{
    using underlying = typename std::underlying_type<Enum>::type;
    return lhs = static_cast<Enum>(static_cast<underlying>(lhs) | static_cast<underlying>(rhs));
}

template<typename Enum>
constexpr typename std::enable_if<core::enable_bitmasks<Enum>::enable, Enum>::type & operator &=(Enum & lhs, const Enum & rhs)
{
    using underlying = typename std::underlying_type<Enum>::type;
    return lhs = static_cast<Enum>(static_cast<underlying>(lhs) & static_cast<underlying>(rhs));
}

template<typename Enum>
std::enable_if_t<core::enable_bitmasks<Enum>::enable, bool> operator !(const Enum & lhs)
{
    using underlying = typename std::underlying_type<Enum>::type;
    return static_cast<underlying>(lhs) == 0;
}

template<typename Enum>
constexpr typename std::enable_if<core::enable_bitmasks<Enum>::enable, Enum>::type operator ~(const Enum & lhs)
{
    using underlying = typename std::underlying_type<Enum>::type;
    return static_cast<Enum>(~static_cast<underlying>(lhs));
}

