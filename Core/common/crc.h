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
	};
}