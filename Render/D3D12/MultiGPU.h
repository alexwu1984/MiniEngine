#pragma once
#include "core/inc.h"
#include "math/math.h"

namespace RenderCore
{
#define WITH_SLI 0	// Implicit SLI
#define WITH_MGPU 0	// Explicit MGPU
#define MAX_NUM_GPUS 4
	extern  uint32_t GNumExplicitGPUsForRendering;
	extern  uint32_t GNumAlternateFrameRenderingGroups;

	/** A mask where each bit is a GPU index. Can not be empty so that non SLI platforms can optimize it to be always 1.  */
	struct FRHIGPUMask
	{
	private:
		uint32_t GPUMask;

	public:
		FORCEINLINE explicit FRHIGPUMask(uint32_t InGPUMask) : GPUMask(InGPUMask)
		{
#if WITH_MGPU
			assert(InGPUMask != 0);
#else
			assert(InGPUMask == 1);
#endif
		}

		FORCEINLINE FRHIGPUMask() : GPUMask(FRHIGPUMask::GPU0())
		{
		}

		FORCEINLINE static FRHIGPUMask FromIndex(uint32_t GPUIndex) { return FRHIGPUMask(1 << GPUIndex); }

		FORCEINLINE uint32_t ToIndex() const
		{
#if WITH_MGPU
			assert(HasSingleIndex());
			return math::CountTrailingZeros(GPUMask);
#else
			return 0;
#endif
		}

		FORCEINLINE bool HasSingleIndex() const
		{
#if WITH_MGPU
			return math::IsPowerOfTwo(GPUMask);
#else
			return true;
#endif
		}

		FORCEINLINE uint32_t GetLastIndex() const
		{
#if WITH_MGPU
			return math::FloorLog2(GPUMask);
#else
			return 0;
#endif
		}

		FORCEINLINE uint32_t GetFirstIndex() const
		{
#if WITH_MGPU
			return math::CountTrailingZeros(GPUMask);
#else
			return 0;
#endif
		}

		FORCEINLINE bool Contains(uint32_t GPUIndex) const { return (GPUMask & (1 << GPUIndex)) != 0; }
		FORCEINLINE bool Intersects(const FRHIGPUMask& Rhs) const { return (GPUMask & Rhs.GPUMask) != 0; }

		FORCEINLINE bool operator ==(const FRHIGPUMask& Rhs) const { return GPUMask == Rhs.GPUMask; }

		void operator |=(const FRHIGPUMask& Rhs) { GPUMask |= Rhs.GPUMask; }
		void operator &=(const FRHIGPUMask& Rhs) { GPUMask &= Rhs.GPUMask; }
		FORCEINLINE operator uint32_t() const { return GPUMask; }

		FORCEINLINE FRHIGPUMask operator &(const FRHIGPUMask& Rhs) const
		{
			return FRHIGPUMask(GPUMask & Rhs.GPUMask);
		}

		FORCEINLINE FRHIGPUMask operator |(const FRHIGPUMask& Rhs) const
		{
			return FRHIGPUMask(GPUMask | Rhs.GPUMask);
		}

		FORCEINLINE static const FRHIGPUMask GPU0() { return FRHIGPUMask(1); }
		FORCEINLINE static const FRHIGPUMask All() { return FRHIGPUMask((1 << GNumExplicitGPUsForRendering) - 1); }

		struct FIterator
		{
			FORCEINLINE FIterator(const uint32_t InGPUMask) : GPUMask(InGPUMask), FirstGPUIndexInMask(0)
			{
#if WITH_MGPU
				FirstGPUIndexInMask = math::CountTrailingZeros(InGPUMask);
#endif
			}

			FORCEINLINE void operator++()
			{
#if WITH_MGPU
				GPUMask &= ~(1 << FirstGPUIndexInMask);
				FirstGPUIndexInMask = math::CountTrailingZeros(GPUMask);
#else
				GPUMask = 0;
#endif
			}

			FORCEINLINE uint32_t operator*() const { return FirstGPUIndexInMask; }
			FORCEINLINE bool operator !=(const FIterator& Rhs) const { return GPUMask != Rhs.GPUMask; }
			FORCEINLINE explicit operator bool() const { return GPUMask != 0; }
			FORCEINLINE bool operator !() const { return !(bool)*this; }

		private:
			uint32_t GPUMask;
			unsigned long FirstGPUIndexInMask;
		};

		FORCEINLINE friend FRHIGPUMask::FIterator begin(const FRHIGPUMask& NodeMask) { return FRHIGPUMask::FIterator(NodeMask.GPUMask); }
		FORCEINLINE friend FRHIGPUMask::FIterator end(const FRHIGPUMask& NodeMask) { return FRHIGPUMask::FIterator(0); }
	};
}