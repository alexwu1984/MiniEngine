#pragma once
#include "common/bytes_swap.h"

namespace core
{
	struct Crc
	{
		/** lookup table with precalculated CRC values - slicing by 8 implementation */
		static uint32_t CRCTablesSB8[8][256];

		/** generates CRC hash of the memory area */
		static uint32_t MemCrc32(const void* InData, int32_t Length, uint32_t CRC = 0);
		static size_t HashRange(const uint32_t* const Begin, const uint32_t* const End, size_t Hash);

		template <typename T> static inline size_t HashState(const T* StateDesc, size_t Count = 1, size_t Hash = 2166136261U)
		{
			static_assert((sizeof(T) & 3) == 0 && alignof(T) >= 4, "State object is not word-aligned");
			return HashRange((uint32_t*)StateDesc, (uint32_t*)(StateDesc + Count), Hash);
		}
	};
}