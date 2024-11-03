#pragma once
#define EPSILON_E4 (float)(1E-4)

namespace math
{
	#ifndef FORCEINLINE
		#define FORCEINLINE __forceinline							/* Force code to be inline */
	#endif

	enum class Intersect
	{
		E_Intersect,
		E_NoIntersect,
		E_Out,
		E_In,
		E_Front,
		E_Back,
		E_On,//点在面里面
	};

	template<typename T>
	inline T Clamp(const T& x, const T& low, const T& high)
	{
		return x < low ? low : (x > high ? high : x);
	}

	inline float Lerp(float a, float b, float f)
	{
		return a + f * (b - a);
	}

	const float MATH_PI = 3.1415926535897932f;	/* Extra digits if needed: 3.1415926535897932384626433832795f */
	const float MATH_2PI = 2.f * MATH_PI;
	//const float MATH_PI_HALF = 0.5f * MATH_PI;
	//const float MATH_PI_OVER2 = MATH_PI / 2.0f;
	const float MATH_INFINITY = std::numeric_limits<float>::infinity();
	const float MATH_NEG_INFINITY = -std::numeric_limits<float>::infinity();
	const float INV_PI = 0.31830988618f;
	const float HALF_PI = 1.57079632679f;

	const float DELTA = 0.00001f;
	const float KINDA_SMALL_NUMBER = 1.e-8f;
	const float FLOAT_NON_FRACTIONAL = 8388608.f;

	template <typename T>
	static FORCEINLINE bool IsPowerOfTwo(T Value)
	{
		return ((Value & (Value - 1)) == (T)0);
	}

	/** Multiples value by itself */
	template< class T >
	static FORCEINLINE T Square(const T A)
	{
		return A * A;
	}

	/** Computes absolute value in a generic way */
	template< class T >
	static constexpr FORCEINLINE T Abs(const T A)
	{
		return (A >= (T)0) ? A : -A;
	}

	/** Returns 1, 0, or -1 depending on relation of T to 0 */
	template< class T >
	static constexpr FORCEINLINE T Sign(const T A)
	{
		return (A > (T)0) ? (T)1 : ((A < (T)0) ? (T)-1 : (T)0);
	}

	/** Returns higher value in a generic way */
	template< class T >
	static constexpr FORCEINLINE T Max(const T A, const T B)
	{
		return (A >= B) ? A : B;
	}

	/** Returns lower value in a generic way */
	template< class T >
	static constexpr FORCEINLINE T Min(const T A, const T B)
	{
		return (A <= B) ? A : B;
	}

	/** Returns highest of 3 values */
	template< class T >
	static FORCEINLINE T Max3(const T A, const T B, const T C)
	{
		return Max(Max(A, B), C);
	}

	/** Returns lowest of 3 values */
	template< class T >
	static FORCEINLINE T Min3(const T A, const T B, const T C)
	{
		return Min(Min(A, B), C);
	}

	/**
 * Converts a float to an integer with truncation towards zero.
 * @param F		Floating point value to convert
 * @return		Truncated integer.
 */
	static constexpr FORCEINLINE int32_t TruncToInt(float F)
	{
		return (int32_t)F;
	}

	/**
	 * Converts a float to an integer value with truncation towards zero.
	 * @param F		Floating point value to convert
	 * @return		Truncated integer value.
	 */
	static FORCEINLINE float TruncToFloat(float F)
	{
		return std::truncf(F);
	}

	/**
	 * Converts a double to an integer value with truncation towards zero.
	 * @param F		Floating point value to convert
	 * @return		Truncated integer value.
	 */
	static FORCEINLINE double TruncToDouble(double F)
	{
		return std::trunc(F);
	}

	/**
	 * Converts a float to a nearest less or equal integer.
	 * @param F		Floating point value to convert
	 * @return		An integer less or equal to 'F'.
	 */
	static FORCEINLINE int32_t FloorToInt(float F)
	{
		return TruncToInt(std::floorf(F));
	}

	/**
	* Converts a float to the nearest less or equal integer.
	* @param F		Floating point value to convert
	* @return		An integer less or equal to 'F'.
	*/
	static FORCEINLINE float FloorToFloat(float F)
	{
		return std::floorf(F);
	}

	/**
	* Converts a double to a less or equal integer.
	* @param F		Floating point value to convert
	* @return		The nearest integer value to 'F'.
	*/
	static FORCEINLINE double FloorToDouble(double F)
	{
		return std::floor(F);
	}

	/**
	 * Converts a float to the nearest integer. Rounds up when the fraction is .5
	 * @param F		Floating point value to convert
	 * @return		The nearest integer to 'F'.
	 */
	static FORCEINLINE int32_t RoundToInt(float F)
	{
		return FloorToInt(F + 0.5f);
	}

	/**
	* Converts a float to the nearest integer. Rounds up when the fraction is .5
	* @param F		Floating point value to convert
	* @return		The nearest integer to 'F'.
	*/
	static FORCEINLINE float RoundToFloat(float F)
	{
		return FloorToFloat(F + 0.5f);
	}

	/**
	* Converts a double to the nearest integer. Rounds up when the fraction is .5
	* @param F		Floating point value to convert
	* @return		The nearest integer to 'F'.
	*/
	static FORCEINLINE double RoundToDouble(double F)
	{
		return FloorToDouble(F + 0.5);
	}

	/**
	* Converts a float to the nearest greater or equal integer.
	* @param F		Floating point value to convert
	* @return		An integer greater or equal to 'F'.
	*/
	static FORCEINLINE int32_t CeilToInt(float F)
	{
		return TruncToInt(std::ceilf(F));
	}

	/**
	* Converts a float to the nearest greater or equal integer.
	* @param F		Floating point value to convert
	* @return		An integer greater or equal to 'F'.
	*/
	static FORCEINLINE float CeilToFloat(float F)
	{
		return std::ceilf(F);
	}

	/**
	* Converts a double to the nearest greater or equal integer.
	* @param F		Floating point value to convert
	* @return		An integer greater or equal to 'F'.
	*/
	static FORCEINLINE double CeilToDouble(double F)
	{
		return std::ceil(F);
	}

	/**
	* Returns signed fractional part of a float.
	* @param Value	Floating point value to convert
	* @return		A float between >=0 and < 1 for nonnegative input. A float between >= -1 and < 0 for negative input.
	*/
	static FORCEINLINE float Fractional(float Value)
	{
		return Value - TruncToFloat(Value);
	}

	/**
	* Returns the fractional part of a float.
	* @param Value	Floating point value to convert
	* @return		A float between >=0 and < 1.
	*/
	static FORCEINLINE float Frac(float Value)
	{
		return Value - FloorToFloat(Value);
	}

	/**
* Returns the floating-point remainder of X / Y
* Warning: Always returns remainder toward 0, not toward the smaller multiple of Y.
*			So for example Fmod(2.8f, 2) gives .8f as you would expect, however, Fmod(-2.8f, 2) gives -.8f, NOT 1.2f
* Use Floor instead when snapping positions that can be negative to a grid
*
* This is forced to *NOT* inline so that divisions by constant Y does not get optimized in to an inverse scalar multiply,
* which is not consistent with the intent nor with the vectorized version.
*/
	float Fmod(float X, float Y);

	/**
	* Breaks the given value into an integral and a fractional part.
	* @param InValue	Floating point value to convert
	* @param OutIntPart Floating point value that receives the integral part of the number.
	* @return			The fractional part of the number.
	*/
	static FORCEINLINE float Modf(const float InValue, float* OutIntPart)
	{
		return std::modff(InValue, OutIntPart);
	}

	/**
	* Breaks the given value into an integral and a fractional part.
	* @param InValue	Floating point value to convert
	* @param OutIntPart Floating point value that receives the integral part of the number.
	* @return			The fractional part of the number.
	*/
	static FORCEINLINE double Modf(const double InValue, double* OutIntPart)
	{
		return std::modf(InValue, OutIntPart);
	}

	// Returns e^Value
	static FORCEINLINE float Exp(float Value) { return std::expf(Value); }
	// Returns 2^Value
	static FORCEINLINE float Exp2(float Value) { return std::powf(2.f, Value); /*exp2f(Value);*/ }
	static FORCEINLINE float Loge(float Value) { return std::logf(Value); }
	static FORCEINLINE float LogX(float Base, float Value) { return Loge(Value) / Loge(Base); }
	// 1.0 / Loge(2) = 1.4426950f
	static FORCEINLINE float Log2(float Value) { return Loge(Value) * 1.4426950f; }

	static FORCEINLINE float Sin(float Value) { return std::sinf(Value); }
	static FORCEINLINE float Asin(float Value) { return std::asinf((Value < -1.f) ? -1.f : ((Value < 1.f) ? Value : 1.f)); }
	static FORCEINLINE float Sinh(float Value) { return std::sinhf(Value); }
	static FORCEINLINE float Cos(float Value) { return std::cosf(Value); }
	static FORCEINLINE float Acos(float Value) { return std::acosf((Value < -1.f) ? -1.f : ((Value < 1.f) ? Value : 1.f)); }
	static FORCEINLINE float Tan(float Value) { return std::tanf(Value); }
	static FORCEINLINE float Atan(float Value) { return std::atanf(Value); }
	float Atan2(float Y, float X);
	static FORCEINLINE float Sqrt(float Value) { return std::sqrtf(Value); }
	static FORCEINLINE float Pow(float A, float B) { return std::powf(A, B); }

	/** Computes a fully accurate inverse square root */
	static FORCEINLINE float InvSqrt(float F)
	{
		return 1.0f / std::sqrtf(F);
	}

	/** Computes a faster but less accurate inverse square root */
	static FORCEINLINE float InvSqrtEst(float F)
	{
		return InvSqrt(F);
	}

	/** Return true if value is NaN (not a number). */
	static FORCEINLINE bool IsNaN(float A)
	{
		return ((*(uint32_t*)&A) & 0x7FFFFFFFU) > 0x7F800000U;
	}
	static FORCEINLINE bool IsNaN(double A)
	{
		return ((*(uint64_t*)&A) & 0x7FFFFFFFFFFFFFFFULL) > 0x7FF0000000000000ULL;
	}

	/**
	 * Computes the base 2 logarithm for an integer value that is greater than 0.
	 * The result is rounded down to the nearest integer.
	 *
	 * @param Value		The value to compute the log of
	 * @return			Log2 of Value. 0 if Value is 0.
	 */
	static FORCEINLINE uint32_t FloorLog2(uint32_t Value)
	{
		/*		// reference implementation
				// 1500ms on test data
				uint32 Bit = 32;
				for (; Bit > 0;)
				{
					Bit--;
					if (Value & (1<<Bit))
					{
						break;
					}
				}
				return Bit;
		*/
		// same output as reference

		// see http://codinggorilla.domemtech.com/?p=81 or http://en.wikipedia.org/wiki/Binary_logarithm but modified to return 0 for a input value of 0
		// 686ms on test data
		uint32_t pos = 0;
		if (Value >= 1 << 16) { Value >>= 16; pos += 16; }
		if (Value >= 1 << 8) { Value >>= 8; pos += 8; }
		if (Value >= 1 << 4) { Value >>= 4; pos += 4; }
		if (Value >= 1 << 2) { Value >>= 2; pos += 2; }
		if (Value >= 1 << 1) { pos += 1; }
		return (Value == 0) ? 0 : pos;

		// even faster would be method3 but it can introduce more cache misses and it would need to store the table somewhere
		// 304ms in test data
		/*int LogTable256[256];

		void prep()
		{
			LogTable256[0] = LogTable256[1] = 0;
			for (int i = 2; i < 256; i++)
			{
				LogTable256[i] = 1 + LogTable256[i / 2];
			}
			LogTable256[0] = -1; // if you want log(0) to return -1
		}

		int _forceinline method3(uint32 v)
		{
			int r;     // r will be lg(v)
			uint32 tt; // temporaries

			if ((tt = v >> 24) != 0)
			{
				r = (24 + LogTable256[tt]);
			}
			else if ((tt = v >> 16) != 0)
			{
				r = (16 + LogTable256[tt]);
			}
			else if ((tt = v >> 8 ) != 0)
			{
				r = (8 + LogTable256[tt]);
			}
			else
			{
				r = LogTable256[v];
			}
			return r;
		}*/
	}

	/**
 * Computes the base 2 logarithm for a 64-bit value that is greater than 0.
 * The result is rounded down to the nearest integer.
 *
 * @param Value		The value to compute the log of
 * @return			Log2 of Value. 0 if Value is 0.
 */
	static FORCEINLINE uint64_t FloorLog2_64(uint64_t Value)
	{
		uint64_t pos = 0;
		if (Value >= 1ull << 32) { Value >>= 32; pos += 32; }
		if (Value >= 1ull << 16) { Value >>= 16; pos += 16; }
		if (Value >= 1ull << 8) { Value >>= 8; pos += 8; }
		if (Value >= 1ull << 4) { Value >>= 4; pos += 4; }
		if (Value >= 1ull << 2) { Value >>= 2; pos += 2; }
		if (Value >= 1ull << 1) { pos += 1; }
		return (Value == 0) ? 0 : pos;
	}

	/**
	 * Counts the number of leading zeros in the bit representation of the 8-bit value
	 *
	 * @param Value the value to determine the number of leading zeros for
	 *
	 * @return the number of zeros before the first "on" bit
	 */
	static FORCEINLINE uint8_t CountLeadingZeros8(uint8_t Value)
	{
		if (Value == 0) return 8;
		return uint8_t(7 - FloorLog2(uint32_t(Value)));
	}

	/**
	 * Counts the number of leading zeros in the bit representation of the 32-bit value
	 *
	 * @param Value the value to determine the number of leading zeros for
	 *
	 * @return the number of zeros before the first "on" bit
	 */
	static FORCEINLINE uint32_t CountLeadingZeros(uint32_t Value)
	{
		if (Value == 0) return 32;
		return 31 - FloorLog2(Value);
	}

	/**
	 * Counts the number of leading zeros in the bit representation of the 64-bit value
	 *
	 * @param Value the value to determine the number of leading zeros for
	 *
	 * @return the number of zeros before the first "on" bit
	 */
	static FORCEINLINE uint64_t CountLeadingZeros64(uint64_t Value)
	{
		if (Value == 0) return 64;
		return 63 - FloorLog2_64(Value);
	}

	/**
	 * Counts the number of trailing zeros in the bit representation of the value
	 *
	 * @param Value the value to determine the number of trailing zeros for
	 *
	 * @return the number of zeros after the last "on" bit
	 */
	static FORCEINLINE uint32_t CountTrailingZeros(uint32_t Value)
	{
		if (Value == 0)
		{
			return 32;
		}
		uint32_t Result = 0;
		while ((Value & 1) == 0)
		{
			Value >>= 1;
			++Result;
		}
		return Result;
	}

	/**
	 * Counts the number of trailing zeros in the bit representation of the value
	 *
	 * @param Value the value to determine the number of trailing zeros for
	 *
	 * @return the number of zeros after the last "on" bit
	 */
	static FORCEINLINE uint64_t CountTrailingZeros64(uint64_t Value)
	{
		if (Value == 0)
		{
			return 64;
		}
		uint64_t Result = 0;
		while ((Value & 1) == 0)
		{
			Value >>= 1;
			++Result;
		}
		return Result;
	}

	/**
	 * Returns smallest N such that (1<<N)>=Arg.
	 * Note: CeilLogTwo(0)=0 because (1<<0)=1 >= 0.
	 */
	static FORCEINLINE uint32_t CeilLogTwo(uint32_t Arg)
	{
		int32_t Bitmask = ((int32_t)(CountLeadingZeros(Arg) << 26)) >> 31;
		return (32 - CountLeadingZeros(Arg - 1)) & (~Bitmask);
	}

	static FORCEINLINE uint64_t CeilLogTwo64(uint64_t Arg)
	{
		int64_t Bitmask = ((int64_t)(CountLeadingZeros64(Arg) << 57)) >> 63;
		return (64 - CountLeadingZeros64(Arg - 1)) & (~Bitmask);
	}

	/** @return Rounds the given number up to the next highest power of two. */
	static FORCEINLINE uint32_t RoundUpToPowerOfTwo(uint32_t Arg)
	{
		return 1 << CeilLogTwo(Arg);
	}

	static FORCEINLINE uint64_t RoundUpToPowerOfTwo64(uint64_t V)
	{
		return uint64_t(1) << CeilLogTwo64(V);
	}

	/** Returns a random integer between 0 and RAND_MAX, inclusive */
	static FORCEINLINE int32_t Rand() { return rand(); }

	/** Seeds global random number functions Rand() and FRand() */
	static FORCEINLINE void RandInit(int32_t Seed) { srand(Seed); }

	/** Returns a random float between 0 and 1, inclusive. */
	static FORCEINLINE float FRand()
	{
		// FP32 mantissa can only represent 24 bits before losing precision
		constexpr int32_t RandMax = 0x00ffffff < RAND_MAX ? 0x00ffffff : RAND_MAX;
		return (Rand() & RandMax) / (float)RandMax;
	}

	/** Seeds future calls to SRand() */
	void SRandInit(int32_t Seed);

	/** Returns the current seed for SRand(). */
	int32_t GetRandSeed();

	/** Returns a seeded random float in the range [0,1), using the seed from SRandInit(). */
	float SRand();

	/** Performs a linear interpolation between two values, Alpha ranges from 0-1 */
	template< class T, class U >
	static  T Lerp(const T& A, const T& B, const U& Alpha)
	{
		return (T)(A + Alpha * (B - A));
	}

	/** Performs a linear interpolation between two values, Alpha ranges from 0-1. Handles full numeric range of T */
	template< class T >
	static  T LerpStable(const T& A, const T& B, double Alpha)
	{
		return (T)((A * (1.0 - Alpha)) + (B * Alpha));
	}

	/** Performs a linear interpolation between two values, Alpha ranges from 0-1. Handles full numeric range of T */
	template< class T >
	static  T LerpStable(const T& A, const T& B, float Alpha)
	{
		return (T)((A * (1.0f - Alpha)) + (B * Alpha));
	}

	/** Performs a 2D linear interpolation between four values values, FracX, FracY ranges from 0-1 */
	template< class T, class U >
	static  T BiLerp(const T& P00, const T& P10, const T& P01, const T& P11, const U& FracX, const U& FracY)
	{
		return Lerp(
			Lerp(P00, P10, FracX),
			Lerp(P01, P11, FracX),
			FracY
		);
	}

	static FORCEINLINE void SinCos(float* ScalarSin, float* ScalarCos, float  Value)
	{
		// Map Value to y in [-pi,pi], x = 2*pi*quotient + remainder.
		float quotient = (INV_PI * 0.5f) * Value;
		if (Value >= 0.0f)
		{
			quotient = (float)((int)(quotient + 0.5f));
		}
		else
		{
			quotient = (float)((int)(quotient - 0.5f));
		}
		float y = Value - (2.0f * MATH_PI) * quotient;

		// Map y to [-pi/2,pi/2] with sin(y) = sin(Value).
		float sign;
		if (y > HALF_PI)
		{
			y = MATH_PI - y;
			sign = -1.0f;
		}
		else if (y < -HALF_PI)
		{
			y = -MATH_PI - y;
			sign = -1.0f;
		}
		else
		{
			sign = +1.0f;
		}

		float y2 = y * y;

		// 11-degree minimax approximation
		*ScalarSin = (((((-2.3889859e-08f * y2 + 2.7525562e-06f) * y2 - 0.00019840874f) * y2 + 0.0083333310f) * y2 - 0.16666667f) * y2 + 1.0f) * y;

		// 10-degree minimax approximation
		float p = ((((-2.6051615e-07f * y2 + 2.4760495e-05f) * y2 - 0.0013888378f) * y2 + 0.041666638f) * y2 - 0.5f) * y2 + 1.0f;
		*ScalarCos = sign * p;
	}

	inline float NormalRandom(float mu = 0.f, float sigma = 1.f)
	{
		unsigned seed = (unsigned)std::chrono::steady_clock::now().time_since_epoch().count();
		static std::default_random_engine generator(seed);
		static std::normal_distribution<float> distribution(mu, sigma);
		return distribution(generator);
	}

	inline uint32_t ReverseBits(uint32_t Bits)
	{
		Bits = (Bits << 16) | (Bits >> 16);
		Bits = ((Bits & 0x00ff00ff) << 8) | ((Bits & 0xff00ff00) >> 8);
		Bits = ((Bits & 0x0f0f0f0f) << 4) | ((Bits & 0xf0f0f0f0) >> 4);
		Bits = ((Bits & 0x33333333) << 2) | ((Bits & 0xcccccccc) >> 2);
		Bits = ((Bits & 0x55555555) << 1) | ((Bits & 0xaaaaaaaa) >> 1);
		return Bits;
	}

	inline float Radians(float degrees)
	{
		return degrees * static_cast<float>(0.01745329251994329576923690768489);
	}

	inline float Degrees(float radians)
	{
		return radians * static_cast<float>(57.295779513082320876798154814105);
	}
}