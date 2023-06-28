#pragma once
#include <array>
namespace core
{
    template<typename T>
    class vec2
    {
    public:
        vec2() : x(), y() { }

        explicit vec2(T _t) : x(_t), y(_t) { }

        vec2(T _x, T _y) : x(_x), y(_y) { }

        vec2(const vec2 & vec) : x(vec.x), y(vec.y) { }

        void set(T _x, T _y)
        {
            x = _x;
            y = _y;
        }

        vec2 & expand(T _cx, T _cy)
        {
            w += _cx * 2;
            h += _cy * 2;
            return *this;
        }

        vec2 & offset(const vec2 & off)
        {
            x += off.x;
            y += off.y;
            return *this;
        }

        vec2 & offset(const T & off_x, const T & off_y)
        {
            x += off_x;
            y += off_y;
            return *this;
        }

        vec2 offseted(const vec2 & off) const
        {
            return vec2(x + off.x, y + off.y);
        }

        vec2 offseted(const T & off_x, const T & off_y) const
        {
            return vec2(x + off_x, y + off_y);
        }

        void clear()
        {
            w = 0;
            h = 0;
        }

        bool empty() const
        {
            if constexpr (std::is_floating_point<T>::value)
                return std::isnan(w) || std::isnan(h) || std::abs(w) <= std::numeric_limits<T>::epsilon() || std::abs(h) <= std::numeric_limits<T>::epsilon();
            else
                return w == 0 || h == 0;
        }

        vec2 & operator =(const vec2 & vec)
        {
            x = vec.x;
            y = vec.y;
            return *this;
        }

        //------------------------------------------------------- 四则运算
        template<typename = decltype(std::declval<T>() + std::declval<T>())>
        vec2 operator +(const vec2 & vec) const
        {
            return vec2(x + vec.x, y + vec.y);
        }

        template<typename = decltype(std::declval<T>() - std::declval<T>())>
        vec2 operator -(const vec2 & vec) const
        {
            return vec2(x - vec.x, y - vec.y);
        }

        template<typename = decltype(std::declval<T>() * std::declval<T>())>
        vec2 operator *(const vec2 & vec) const
        {
            return vec2(x * vec.x, y * vec.y);
        }

        template<typename = decltype(std::declval<T>() / std::declval<T>())>
        vec2 operator /(const vec2 & vec) const
        {
            return vec2(x / vec.x, y / vec.y);
        }

        template<typename = decltype(std::declval<T&>() += std::declval<T>())>
        vec2 & operator +=(const vec2 & vec)
        {
            x += vec.x;
            y += vec.y;
            return *this;
        }

        template<typename = decltype(std::declval<T&>() -= std::declval<T>())>
        vec2 & operator -=(const vec2 & vec)
        {
            x -= vec.x;
            y -= vec.y;
            return *this;
        }

        template<typename = decltype(std::declval<T&>() *= std::declval<T>())>
        vec2 & operator *=(const vec2 & vec)
        {
            x *= vec.x;
            y *= vec.y;
            return *this;
        }

        template<typename = decltype(std::declval<T&>() /= std::declval<T>())>
        vec2 & operator /=(const vec2 & vec)
        {
            x /= vec.x;
            y /= vec.y;
            return *this;
        }

        template<typename = decltype(std::declval<T>() * std::declval<T>())>
        vec2 operator * (const T & rate) const
        {
            return vec2(x * rate, y * rate);
        }

        template<typename = std::enable_if_t<!std::is_same_v<T, float>>>
        vec2 operator * (float rate) const
        {
            return vec2(static_cast<T>(x * rate), static_cast<T>(y * rate));
        }

        vec2 operator / (const T & rate) const
        {
            return vec2(x / rate, y / rate);
        }

        template<typename = std::enable_if_t<!std::is_same_v<T, float>>>
        vec2 operator / (float rate) const
        {
            return vec2(static_cast<T>(x / rate), static_cast<T>(y / rate));
        }

        vec2 & operator *=(const T & rate)
        {
            x *= rate;
            y *= rate;
            return *this;
        }

        template<typename = std::enable_if_t<!std::is_same_v<T, float>>>
        vec2 & operator *= (float rate)
        {
            x = static_cast<T>(x * rate);
            y = static_cast<T>(y * rate);
            return *this;
        }

        vec2 & operator /=(const T & rate)
        {
            x /= rate;
            y /= rate;
            return *this;
        }

        template<typename = std::enable_if_t<!std::is_same_v<T, float>>>
        vec2 & operator /=(float rate)
        {
            x = static_cast<T>(x / rate);
            y = static_cast<T>(y / rate);
            return *this;
        }

        vec2 operator -() const
        {
            return vec2(-x, -y);
        }

        vec2 operator +() const
        {
            return *this;
        }

        //--------------------------------------------------比较运算
        bool operator ==(const vec2 & vec) const
        {
            if constexpr (std::is_floating_point_v<T>)
                return std::equal(x, vec.x) && std::equal(y, vec.y);
            else
                return x == vec.x && y == vec.y;
        }

        bool operator !=(const vec2 & vec) const
        {
            return !operator ==(vec);
        }

        bool operator <(const vec2 & vec) const
        {
            return x < vec.x && y < vec.y;
        }

        bool operator >(const vec2 & vec) const
        {
            return x > vec.x && y > vec.y;
        }

        bool operator <=(const vec2 & vec) const
        {
            return x <= vec.x && y <= vec.y;
        }

        bool operator >=(const vec2 & vec) const
        {
            return x >= vec.x && y >= vec.y;
        }

        template<typename T2>
        vec2<T2> to() const
        {
            return { static_cast<T2>(x), static_cast<T2>(y) };
        }

        template<class ConvertT>
        auto convert(ConvertT convert) const
        {
            return vec2(convert(x), convert(y));
        }

    public:
        union
        {
            std::array<T, 2> arr;
            struct
            {
                //! 横坐标。
                T x;
                //! 纵坐标。
                T y;
            };

            struct
            {
                //! 横坐标。
                T left;
                //! 纵坐标。
                T top;
            };

            struct
            {
                //! 宽。
                T cx;
                //! 高。
                T cy;
            };

            struct
            {
                //! 宽。
                T w;
                //! 高。
                T h;
            };

            struct
            {
                //! 列。
                T col;
                //! 行。
                T row;
            };

            struct
            {
                //! 分子
                T num;
                //! 分母
                T den;
            };
        };
    };

    typedef vec2<int> vec2i;
    typedef vec2<unsigned int> vec2u;
    typedef vec2<float> vec2f;
    typedef vec2<double> si64f;
}
