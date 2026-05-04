#include "Render/SkyLightEnvironment.h"
#include "Render/SkyLightIBLPrecomputePrivate.h"
#include "RHI/RHIDefinitions.h"
#include "math/math.h"

namespace Engine
{
	namespace
	{
		// Appoximation of joint Smith term for GGX
		// [Heitz 2014, "Understanding the Masking-Shadowing Function in Microfacet-Based BRDFs"]
		float G_SmithJointApprox(float a2, float NoV, float NoL)
		{
			float a = math::Sqrt(a2);
			float Vis_SmithV = NoL * (NoV * (1 - a) + a);
			float Vis_SmithL = NoV * (NoL * (1 - a) + a);
			return 0.5f / (Vis_SmithV + Vis_SmithL);
		}
	} // namespace

	void FSkyLightIBLPrecompute::GenerateBRDFIntegrationLUT()
	{
		C_P(FSkyLightIBLPrecompute);
		if (d->PreBRDF)
			return;

		const int width = 128;
		const int height = 32;
		std::vector<math::Vector2> ImageData(static_cast<size_t>(width) * static_cast<size_t>(height));

		for (int y = 0; y < height; ++y)
		{
			float Roughness = (float)(y + 0.5f) / height;
			float m = Roughness * Roughness;
			float m2 = m * m;

			for (int x = 0; x < width; ++x)
			{
				float NoV = (float)(x + 0.5f) / width;

				math::Vector3 V;
				V.x = math::Sqrt(1.0f - NoV * NoV);
				V.y = 0.0f;
				V.z = NoV;

				float A = 0.0f;
				float B = 0.0f;

				const uint32_t NumSamples = 128;
				for (uint32_t i = 0; i < NumSamples; i++)
				{
					float E1 = (float)i / NumSamples;
					float E2 = (float)math::ReverseBits(i) / (float)0x100000000LL;

					{
						float Phi = 2.0f * math::MATH_PI * E1;
						float CosTheta = math::Sqrt((1.0f - E2) / (1.0f + (m2 - 1.0f) * E2));
						float SinTheta = math::Sqrt(1.0f - CosTheta * CosTheta);

						math::Vector3 H(SinTheta * math::Cos(Phi), SinTheta * math::Sin(Phi), CosTheta);
						math::Vector3 L = 2.0f * V.Dot(H) * H - V;

						float NoL = std::max(L.z, 0.0f);
						float NoH = std::max(H.z, 0.0f);
						float VoH = std::max(V.Dot(H), 0.0f);

						if (NoL > 0.0f)
						{
							float Vis = G_SmithJointApprox(m2, NoV, NoL);
							float NoL_Vis_PDF = NoL * Vis * (4.f * VoH / NoH);
							float Fc = math::Pow(1.0f - VoH, 5.f);
							A += NoL_Vis_PDF * (1.0f - Fc);
							B += NoL_Vis_PDF * Fc;
						}
					}
				}

				math::Vector2& Texel = ImageData[static_cast<size_t>(y) * static_cast<size_t>(width) + static_cast<size_t>(x)];
				Texel.x = A / NumSamples;
				Texel.y = B / NumSamples;
			}
		}

		d->PreBRDF = d->RHI->RHICreateTexture2D(RenderCore::EPixelFormat::PF_G32R32F, RenderCore::TexCreate_ShaderResource, width, height, 1,
			ImageData.data());
	}

} // namespace Engine
