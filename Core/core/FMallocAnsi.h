#pragma once
#include <cstddef>
#include <cstdint>

/**
 * UE4-style minimal system allocator (FMallocAnsi): malloc / _aligned_malloc on Windows.
 * All actual heap traffic for engine layers should go through this in Debug (via debug_memory)
 * or directly in Release (ascii_memory / FMemory).
 */
class FMallocAnsi
{
public:
	static FMallocAnsi& Get();

	/** @param Alignment 0 = default (malloc/free); otherwise power-of-two for _aligned_malloc. */
	void* Malloc(size_t Size, uint32_t Alignment = 0);

	/** Must use the same Alignment category as Malloc (0 vs non-zero). */
	void Free(void* Ptr, uint32_t Alignment = 0);

private:
	FMallocAnsi() = default;
};

/** Thin helpers in the style of UE FMemory (Malloc/Free only). */
namespace FMemory
{
	inline void* Malloc(size_t Count, uint32_t Alignment = 0)
	{
		return FMallocAnsi::Get().Malloc(Count, Alignment);
	}

	inline void Free(void* Mem, uint32_t Alignment = 0)
	{
		FMallocAnsi::Get().Free(Mem, Alignment);
	}
}
