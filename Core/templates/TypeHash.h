#pragma once

#include "templates/EnableIf.h"
#include "templates/IsEnum.h"
#include "core/strings.h"
/**
 * Combines two hash values to get a third.
 * Note - this function is not commutative.
 */
namespace Templates
{
	inline uint32_t HashCombine(uint32_t A, uint32_t C)
	{
		uint32_t B = 0x9e3779b9;
		A += B;

		A -= B; A -= C; A ^= (C >> 13);
		B -= C; B -= A; B ^= (A << 8);
		C -= A; C -= B; C ^= (B >> 13);
		A -= B; A -= C; A ^= (C >> 12);
		B -= C; B -= A; B ^= (A << 16);
		C -= A; C -= B; C ^= (B >> 5);
		A -= B; A -= C; A ^= (C >> 3);
		B -= C; B -= A; B ^= (A << 10);
		C -= A; C -= B; C ^= (B >> 15);

		return C;
	}


	inline uint32_t PointerHash(const void* Key, uint32_t C = 0)
	{
		// Avoid LHS stalls on PS3 and Xbox 360
#if _WIN64
	// Ignoring the lower 4 bits since they are likely zero anyway.
	// Higher bits are more significant in 64 bit builds.
		auto PtrInt = reinterpret_cast<uint64_t>(Key) >> 4;
#else
		auto PtrInt = reinterpret_cast<uint32_t>(Key);
#endif

		return HashCombine((uint32_t)PtrInt, C);
	}


	//
	// Hash functions for common types.
	//

	inline uint32_t GetTypeHash(const uint8_t A)
	{
		return A;
	}

	inline uint32_t GetTypeHash(const int8_t A)
	{
		return A;
	}

	inline uint32_t GetTypeHash(const uint16_t A)
	{
		return A;
	}

	inline uint32_t GetTypeHash(const int16_t A)
	{
		return A;
	}

	inline uint32_t GetTypeHash(const int32_t A)
	{
		return A;
	}

	inline uint32_t GetTypeHash(const uint32_t A)
	{
		return A;
	}

	inline uint32_t GetTypeHash(const uint64_t A)
	{
		return (uint32_t)A + ((uint32_t)(A >> 32) * 23);
	}

	inline uint32_t GetTypeHash(const int64_t A)
	{
		return (uint32_t)A + ((uint32_t)(A >> 32) * 23);
	}

	inline uint32_t GetTypeHash(float Value)
	{
		return *(uint32_t*)&Value;
	}

	inline uint32_t GetTypeHash(double Value)
	{
		return GetTypeHash(*(uint64_t*)&Value);
	}

	inline uint32_t GetTypeHash(const TCHAR* S)
	{
#if UNICODE
		std::string utf8 = core::ucs2_u8(S);
		return core::HashString(utf8);
#else
		return core::HashString(std::string(S));
#endif
	}

	inline uint32_t GetTypeHash(const void* A)
	{
		return PointerHash(A);
	}

	inline uint32_t GetTypeHash(void* A)
	{
		return PointerHash(A);
	}
}



template <typename EnumType>
FORCEINLINE  typename TEnableIf<TIsEnum<EnumType>::Value, uint32_t>::Type GetTypeHash(EnumType E)
{
	return GetTypeHash((__underlying_type(EnumType))E);
}
