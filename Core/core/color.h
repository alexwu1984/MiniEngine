#pragma once
#include "core/inc.h"
#include "math/math.h"
#include "math/vector4.h"
#include "core/strings.h"

namespace core
{
	struct FColor;

	/**
	 * Enum for the different kinds of gamma spaces we expect to need to convert from/to.
	 */
	enum class EGammaSpace : uint8_t
	{
		/** No gamma correction is applied to this space, the incoming colors are assumed to already be in linear space. */
		Linear,
		/** A simplified sRGB gamma correction is applied, pow(1/2.2). */
		Pow22,
		/** Use the standard sRGB conversion. */
		sRGB,
	};

	/**
 * A linear, 32-bit/component floating point RGBA color.
 */
	struct FLinearColor
	{
		float	R,
			G,
			B,
			A;

		/** Static lookup table used for FColor -> FLinearColor conversion. Pow(2.2) */
		static float Pow22OneOver255Table[256];

		/** Static lookup table used for FColor -> FLinearColor conversion. sRGB */
		static float sRGBToLinearTable[256];

		FORCEINLINE FLinearColor() : R(0), G(0), B(0), A(0) {}
		constexpr FORCEINLINE FLinearColor(float InR, float InG, float InB, float InA = 1.0f) : R(InR), G(InG), B(InB), A(InA) {}

		/**
		 * Converts an FColor which is assumed to be in sRGB space, into linear color space.
		 * @param Color The sRGB color that needs to be converted into linear space.
		 */
		FLinearColor(const FColor& Color);

		FLinearColor(const math::Vector3& Vector);

		explicit FLinearColor(const math::Vector4& Vector);


		// Serializer.


		// Conversions.
		 FColor ToRGBE() const;

		/**
		 * Converts an FColor coming from an observed sRGB output, into a linear color.
		 * @param Color The sRGB color that needs to be converted into linear space.
		 */
		 static FLinearColor FromSRGBColor(const FColor& Color);

		/**
		 * Converts an FColor coming from an observed Pow(1/2.2) output, into a linear color.
		 * @param Color The Pow(1/2.2) color that needs to be converted into linear space.
		 */
		static FLinearColor FromPow22Color(const FColor& Color);

		// Operators.

		FORCEINLINE float& Component(int32_t Index)
		{
			return (&R)[Index];
		}

		FORCEINLINE const float& Component(int32_t Index) const
		{
			return (&R)[Index];
		}

		FORCEINLINE FLinearColor operator+(const FLinearColor& ColorB) const
		{
			return FLinearColor(
				this->R + ColorB.R,
				this->G + ColorB.G,
				this->B + ColorB.B,
				this->A + ColorB.A
			);
		}
		FORCEINLINE FLinearColor& operator+=(const FLinearColor& ColorB)
		{
			R += ColorB.R;
			G += ColorB.G;
			B += ColorB.B;
			A += ColorB.A;
			return *this;
		}

		FORCEINLINE FLinearColor operator-(const FLinearColor& ColorB) const
		{
			return FLinearColor(
				this->R - ColorB.R,
				this->G - ColorB.G,
				this->B - ColorB.B,
				this->A - ColorB.A
			);
		}
		FORCEINLINE FLinearColor& operator-=(const FLinearColor& ColorB)
		{
			R -= ColorB.R;
			G -= ColorB.G;
			B -= ColorB.B;
			A -= ColorB.A;
			return *this;
		}

		FORCEINLINE FLinearColor operator*(const FLinearColor& ColorB) const
		{
			return FLinearColor(
				this->R * ColorB.R,
				this->G * ColorB.G,
				this->B * ColorB.B,
				this->A * ColorB.A
			);
		}
		FORCEINLINE FLinearColor& operator*=(const FLinearColor& ColorB)
		{
			R *= ColorB.R;
			G *= ColorB.G;
			B *= ColorB.B;
			A *= ColorB.A;
			return *this;
		}

		FORCEINLINE FLinearColor operator*(float Scalar) const
		{
			return FLinearColor(
				this->R * Scalar,
				this->G * Scalar,
				this->B * Scalar,
				this->A * Scalar
			);
		}

		FORCEINLINE FLinearColor& operator*=(float Scalar)
		{
			R *= Scalar;
			G *= Scalar;
			B *= Scalar;
			A *= Scalar;
			return *this;
		}

		FORCEINLINE FLinearColor operator/(const FLinearColor& ColorB) const
		{
			return FLinearColor(
				this->R / ColorB.R,
				this->G / ColorB.G,
				this->B / ColorB.B,
				this->A / ColorB.A
			);
		}
		FORCEINLINE FLinearColor& operator/=(const FLinearColor& ColorB)
		{
			R /= ColorB.R;
			G /= ColorB.G;
			B /= ColorB.B;
			A /= ColorB.A;
			return *this;
		}

		FORCEINLINE FLinearColor operator/(float Scalar) const
		{
			const float	InvScalar = 1.0f / Scalar;
			return FLinearColor(
				this->R * InvScalar,
				this->G * InvScalar,
				this->B * InvScalar,
				this->A * InvScalar
			);
		}
		FORCEINLINE FLinearColor& operator/=(float Scalar)
		{
			const float	InvScalar = 1.0f / Scalar;
			R *= InvScalar;
			G *= InvScalar;
			B *= InvScalar;
			A *= InvScalar;
			return *this;
		}

		// clamped in 0..1 range
		FORCEINLINE FLinearColor GetClamped(float InMin = 0.0f, float InMax = 1.0f) const
		{
			FLinearColor Ret;

			Ret.R = math::Clamp(R, InMin, InMax);
			Ret.G = math::Clamp(G, InMin, InMax);
			Ret.B = math::Clamp(B, InMin, InMax);
			Ret.A = math::Clamp(A, InMin, InMax);

			return Ret;
		}

		/** Comparison operators */
		FORCEINLINE bool operator==(const FLinearColor& ColorB) const
		{
			return this->R == ColorB.R && this->G == ColorB.G && this->B == ColorB.B && this->A == ColorB.A;
		}
		FORCEINLINE bool operator!=(const FLinearColor& Other) const
		{
			return this->R != Other.R || this->G != Other.G || this->B != Other.B || this->A != Other.A;
		}

		// Error-tolerant comparison.
		FORCEINLINE bool Equals(const FLinearColor& ColorB, float Tolerance = math::KINDA_SMALL_NUMBER) const
		{
			return math::Abs(this->R - ColorB.R) < Tolerance && math::Abs(this->G - ColorB.G) < Tolerance && math::Abs(this->B - ColorB.B) < Tolerance && math::Abs(this->A - ColorB.A) < Tolerance;
		}

		 FLinearColor CopyWithNewOpacity(float NewOpacicty) const
		{
			FLinearColor NewCopy = *this;
			NewCopy.A = NewOpacicty;
			return NewCopy;
		}


		/**
		 * Converts byte hue-saturation-brightness to floating point red-green-blue.
		 */
		static  FLinearColor MakeFromHSV8(uint8_t H, uint8_t S, uint8_t V);

		/**
		* Makes a random but quite nice color.
		*/
		static  FLinearColor MakeRandomColor();

		/**
		* Converts temperature in Kelvins of a black body radiator to RGB chromaticity.
		*/
		static FLinearColor MakeFromColorTemperature(float Temp);

		/**
		 * Euclidean distance between two points.
		 */
		static inline float Dist(const FLinearColor& V1, const FLinearColor& V2)
		{
			return math::Sqrt(math::Square(V2.R - V1.R) + math::Square(V2.G - V1.G) + math::Square(V2.B - V1.B) + math::Square(V2.A - V1.A));
		}


		/** Converts a linear space RGB color to an HSV color */
		FLinearColor LinearRGBToHSV() const;

		/** Converts an HSV color to a linear space RGB color */
		FLinearColor HSVToLinearRGB() const;

		/**
		 * Linearly interpolates between two colors by the specified progress amount.  The interpolation is performed in HSV color space
		 * taking the shortest path to the new color's hue.  This can give better results than FMath::Lerp(), but is much more expensive.
		 * The incoming colors are in RGB space, and the output color will be RGB.  The alpha value will also be interpolated.
		 *
		 * @param	From		The color and alpha to interpolate from as linear RGBA
		 * @param	To			The color and alpha to interpolate to as linear RGBA
		 * @param	Progress	Scalar interpolation amount (usually between 0.0 and 1.0 inclusive)
		 * @return	The interpolated color in linear RGB space along with the interpolated alpha value
		 */
		static  FLinearColor LerpUsingHSV(const FLinearColor& From, const FLinearColor& To, const float Progress);

		/** Quantizes the linear color and returns the result as a FColor.  This bypasses the SRGB conversion. */
		FColor Quantize() const;

		/** Quantizes the linear color with rounding and returns the result as a FColor.  This bypasses the SRGB conversion. */
		FColor QuantizeRound() const;

		/** Quantizes the linear color and returns the result as a FColor with optional sRGB conversion and quality as goal. */
		FColor ToFColor(const bool bSRGB) const;

		/**
		 * Returns a desaturated color, with 0 meaning no desaturation and 1 == full desaturation
		 *
		 * @param	Desaturation	Desaturation factor in range [0..1]
		 * @return	Desaturated color
		 */
		FLinearColor Desaturate(float Desaturation) const;

		/** Computes the perceptually weighted luminance value of a color. */
		inline float ComputeLuminance() const
		{
			return R * 0.3f + G * 0.59f + B * 0.11f;
		}

		/**
		 * Returns the maximum value in this color structure
		 *
		 * @return The maximum color channel value
		 */
		FORCEINLINE float GetMax() const
		{
			return math::Max(math::Max(math::Max(R, G), B), A);
		}

		/** useful to detect if a light contribution needs to be rendered */
		bool IsAlmostBlack() const
		{
			return math::Square(R) < math::DELTA && math::Square(G) < math::DELTA && math::Square(B) < math::DELTA;
		}

		/**
		 * Returns the minimum value in this color structure
		 *
		 * @return The minimum color channel value
		 */
		FORCEINLINE float GetMin() const
		{
			return math::Min(math::Min(math::Min(R, G), B), A);
		}

		FORCEINLINE float GetLuminance() const
		{
			return R * 0.3f + G * 0.59f + B * 0.11f;
		}


		// Common colors.	
		static  const FLinearColor White;
		static  const FLinearColor Gray;
		static  const FLinearColor Black;
		static  const FLinearColor Transparent;
		static  const FLinearColor Red;
		static  const FLinearColor Green;
		static  const FLinearColor Blue;
		static  const FLinearColor Yellow;
	};

	FORCEINLINE uint32_t GetTypeHash(const FLinearColor& LinearColor)
	{
		// Note: this assumes there's no padding in FLinearColor that could contain uncompared data.
		return (uint32_t)core::Hash(&LinearColor, sizeof(FLinearColor));
	}

	FORCEINLINE FLinearColor operator*(float Scalar, const FLinearColor& Color)
	{
		return Color.operator*(Scalar);
	}


	//
//	FColor
//	Stores a color with 8 bits of precision per channel.  
//	Note: Linear color values should always be converted to gamma space before stored in an FColor, as 8 bits of precision is not enough to store linear space colors!
//	This can be done with FLinearColor::ToFColor(true) 
//

	struct FColor
	{
	public:
		// Variables.
		union { struct { uint8_t B, G, R, A; }; uint8_t AlignmentDummy; };

		uint8_t& DWColor(void) { return *((uint8_t*)this); }
		const uint8_t& DWColor(void) const { return *((uint8_t*)this); }

		// Constructors.
		FORCEINLINE FColor()
		{
			// put these into the body for proper ordering with INTEL vs non-INTEL_BYTE_ORDER
			R = G = B = A = 0;
		}
		constexpr FORCEINLINE FColor(uint8_t InR, uint8_t InG, uint8_t InB, uint8_t InA = 255)
			// put these into the body for proper ordering with INTEL vs non-INTEL_BYTE_ORDER
			: B(InB), G(InG), R(InR), A(InA)
		{}

		FORCEINLINE explicit FColor(uint32_t InColor)
		{
			DWColor() = InColor;
		}

		// Operators.
		FORCEINLINE bool operator==(const FColor& C) const
		{
			return DWColor() == C.DWColor();
		}

		FORCEINLINE bool operator!=(const FColor& C) const
		{
			return DWColor() != C.DWColor();
		}

		FORCEINLINE void operator+=(const FColor& C)
		{
			R = (uint8_t)math::Min((int32_t)R + (int32_t)C.R, 255);
			G = (uint8_t)math::Min((int32_t)G + (int32_t)C.G, 255);
			B = (uint8_t)math::Min((int32_t)B + (int32_t)C.B, 255);
			A = (uint8_t)math::Min((int32_t)A + (int32_t)C.A, 255);
		}

		FLinearColor FromRGBE() const;

		static FLinearColor MakeFromHSV8(uint8_t H, uint8_t S, uint8_t V);
		/**
		 * Makes a random but quite nice color.
		 */
		static FColor MakeRandomColor();

		/**
		 * Makes a color red->green with the passed in scalar (e.g. 0 is red, 1 is green)
		 */
		static FColor MakeRedToGreenColorFromScalar(float Scalar);

		/**
		* Converts temperature in Kelvins of a black body radiator to RGB chromaticity.
		*/
		static FColor MakeFromColorTemperature(float Temp);

		/**
		 *	@return a new FColor based of this color with the new alpha value.
		 *	Usage: const FColor& MyColor = FColorList::Green.WithAlpha(128);
		 */
		FColor WithAlpha(uint8_t Alpha) const
		{
			return FColor(R, G, B, Alpha);
		}

		/**
		 * Reinterprets the color as a linear color.
		 *
		 * @return The linear color representation.
		 */
		FORCEINLINE FLinearColor ReinterpretAsLinear() const
		{
			return FLinearColor(R / 255.f, G / 255.f, B / 255.f, A / 255.f);
		}

		/**
		 * Gets the color in a packed uint32 format packed in the order ARGB.
		 */
		FORCEINLINE uint32_t ToPackedARGB() const
		{
			return (A << 24) | (R << 16) | (G << 8) | (B << 0);
		}

		/**
		 * Gets the color in a packed uint32 format packed in the order ABGR.
		 */
		FORCEINLINE uint32_t ToPackedABGR() const
		{
			return (A << 24) | (B << 16) | (G << 8) | (R << 0);
		}

		/**
		 * Gets the color in a packed uint32 format packed in the order RGBA.
		 */
		FORCEINLINE uint32_t ToPackedRGBA() const
		{
			return (R << 24) | (G << 16) | (B << 8) | (A << 0);
		}

		/**
		 * Gets the color in a packed uint32 format packed in the order BGRA.
		 */
		FORCEINLINE uint32_t ToPackedBGRA() const
		{
			return (B << 24) | (G << 16) | (R << 8) | (A << 0);
		}

		/** Some pre-inited colors, useful for debug code */
		static const FColor White;
		static const FColor Black;
		static const FColor Transparent;
		static const FColor Red;
		static const FColor Green;
		static const FColor Blue;
		static const FColor Yellow;
		static const FColor Cyan;
		static const FColor Magenta;
		static const FColor Orange;
		static const FColor Purple;
		static const FColor Turquoise;
		static const FColor Silver;
		static const FColor Emerald;

	private:
		/**
		 * Please use .ToFColor(true) on FLinearColor if you wish to convert from FLinearColor to FColor,
		 * with proper sRGB conversion applied.
		 *
		 * Note: Do not implement or make public.  We don't want people needlessly and implicitly converting between
		 * FLinearColor and FColor.  It's not a free conversion.
		 */
		explicit FColor(const FLinearColor& LinearColor);
	};

	FORCEINLINE uint32_t GetTypeHash(const FColor& Color)
	{
		return Color.DWColor();
	}
}