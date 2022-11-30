#pragma once

#define NOMINMAX

#include <stdio.h>
#include <tchar.h>
#include <stdint.h>
#include <assert.h>

#include <cctype>
#include <cwctype>
#include <locale>
#include <string>

#include <set>
#include <bitset>
#include <array>
#include <vector>
#include <map>
#include <unordered_map>
#include <list>
#include <memory>
#include <map>
#include <queue>
#include <stack>

#include <sstream>
#include <fstream>
#include <iostream>

#include <numeric>
#include <functional>
#include <algorithm> 
#include <regex>
#include <random>

#include <mutex>
#include <atomic>
#include <future>
#include <thread>
#include <chrono>

#include <filesystem>

#if defined(_HAS_CXX17) && _HAS_CXX17
#include <variant>
#include <optional>
#include <any>
#endif _HAS_CXX17

using namespace std::chrono_literals;

#define USE_UTF8

typedef unsigned char byte_t;

#ifndef _WIN64
typedef int32_t intx_t;
typedef uint32_t uintx_t;
typedef int16_t intx_h;
typedef uint16_t uintx_h;
#else
#define BIT64
typedef int64_t intx_t;
typedef uint64_t uintx_t;
typedef int32_t intx_h;
typedef uint32_t uintx_h;
#endif

#include "./memory_manager.h"
#include "./error.h"
#include "./bitflag.h"
#include "./pixel_format.h"
#include "math/math.h"
#include "./vec2.h"
#include "./vec4.h"

#if !_HAS_CXX17
namespace std
{
    template<class _Ty,
        class _Pr>
        _NODISCARD constexpr const _Ty& clamp(const _Ty& _Val, const _Ty& _Min_val, const _Ty& _Max_val, _Pr _Pred)
    {	// returns _Val constrained to [_Min_val, _Max_val] ordered by _Pred
#if _ITERATOR_DEBUG_LEVEL == 2
        if (_DEBUG_LT_PRED(_Pred, _Max_val, _Min_val))
        {
            _STL_REPORT_ERROR("invalid bounds arguments passed to std::clamp");
            return (_Val);
        }
#endif /* _ITERATOR_DEBUG_LEVEL == 2 */

        return (_DEBUG_LT_PRED(_Pred, _Max_val, _Val)
            ? _Max_val
            : _DEBUG_LT_PRED(_Pred, _Val, _Min_val)
            ? _Min_val
            : _Val);
    }

    template<class _Ty>
    _NODISCARD constexpr const _Ty& clamp(const _Ty& _Val, const _Ty& _Min_val, const _Ty& _Max_val)
    {	// returns _Val constrained to [_Min_val, _Max_val]
        return (_STD clamp(_Val, _Min_val, _Max_val, less<>()));
    }
}
#endif

namespace win32 {
    extern void DoAssert(bool success, const wchar_t* file_name, int line);
}

#define WIDE2(x) L##x
#define WIDE1(x) WIDE2(x)
#define WFILE WIDE1(__FILE__)

#ifdef _DEBUG
#define Assert(s) win32::DoAssert(s, WFILE, __LINE__)
#else
#define Assert(s)
#endif
