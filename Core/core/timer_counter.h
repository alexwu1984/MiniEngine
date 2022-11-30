#pragma once

#include "core/inc.h"

namespace core
{
    template<typename ValT, int32_t count>
    class timer_counter
    {
    public:
        timer_counter(std::chrono::microseconds period = 1s) : _period(period)
        {
            for (auto & val : _vals)
                val = 0;
        }

        void acc(ValT val)
        {
            std::chrono::steady_clock::time_point tp_now = std::chrono::high_resolution_clock::now();
            if (tp_now - _tp_last > _period)
            {
                _vals[_index++ % count] = _val;
                _val = 0;
                _tp_last = tp_now;
            }
            _val += val;
        }
        ValT avg() const
        {
            return std::accumulate(_vals.begin(), _vals.end(), (ValT)0) / _vals.size();
        }


    private:
        std::chrono::microseconds _period;
        std::chrono::steady_clock::time_point _tp_last;
        std::array<ValT, count> _vals;

        int64_t _index = 0;
        ValT _val = 0;
    };

    template<typename ValT, int32_t count>
    class average_counter
    {
    public:
        average_counter() { std::fill(_vals.begin(), _vals.end(), ValT()); }
        void acc(ValT val)
        {
            _vals[_index++ % count] = val;
        }
        ValT avg() const
        {
            return std::accumulate(_vals.begin(), _vals.end(), (ValT)0) / _vals.size();
        }

        ValT min() const
        {
            return *std::min_element(_vals.begin(), _vals.end());
        }

        ValT max() const
        {
            return *std::max_element(_vals.begin(), _vals.end());
        }

    private:
        std::array<ValT, count> _vals;
        int64_t _index = 0;
    };
}
