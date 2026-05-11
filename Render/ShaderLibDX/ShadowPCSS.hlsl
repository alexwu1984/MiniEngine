// PCSS for directional shadow map (R32 linear/hardware depth in [0,1], same space as LightViewProj clip z/w).
// Expects Texture2D ShadowMap, SamplerState SampleShadow, GetMainLight() from PerFrameStruct.hlsl.

#ifndef MINIENGINE_SHADOW_PCSS_HLSL
#define MINIENGINE_SHADOW_PCSS_HLSL

static const float kPCSSBlockerSearchRadiusUV = 0.010;
// Wider minimum PCF radius reduces shimmering / moire on large receivers (e.g. dense tessellated floors).
static const float kPCSSMinFilterRadiusUV = 0.00065;
static const float kPCSSMaxFilterRadiusUV = 0.0045;
static const float kPCSSPenumbraMul = 14.0;

static const float2 kPoissonDisk16[16] =
{
	float2(-0.94201624, -0.39906216), float2(0.94558609, -0.76890725), float2(-0.09418410, -0.92938870),
	float2(0.34495938, 0.29387760), float2(-0.91588581, 0.45771432), float2(-0.81544232, -0.87912464),
	float2(-0.38277543, 0.27676845), float2(0.97484398, 0.75648379), float2(0.44323325, -0.97511554),
	float2(0.53742981, -0.47373420), float2(-0.26496911, -0.41893023), float2(0.79197514, 0.19090188),
	float2(-0.24188840, 0.99706507), float2(-0.81409955, 0.91437590), float2(0.19984126, 0.78641367),
	float2(0.14383161, -0.14100790)
};

float ShadowDepthBiasPCSS(float3 Normal)
{
	float3 L = normalize(GetMainLight().Direction);
	float3 n = normalize(Normal);
	float NdotL = abs(dot(n, L));
	const float baseBias = 0.00038;
	const float slopeBias = 0.00135;
	// Direction is toward the light; |L.y| small near horizon → grazing rays → depth aliasing / false umbra.
	float grazing = saturate(1.0 - abs(L.y));
	const float horizonBias = 0.0024 * grazing;
	return baseBias + slopeBias * (1.0 - NdotL) + horizonBias;
}

float PCF_ShadowR32(float2 uvCenter, float zReceiver, float radiusUV, float bias)
{
	float lit = 0.0;
	[unroll]
	for (int i = 0; i < 16; ++i)
	{
		float2 suv = uvCenter + kPoissonDisk16[i] * radiusUV;
		suv = clamp(suv, float2(1e-4, 1e-4), float2(1.0 - 1e-4, 1.0 - 1e-4));
		float d = ShadowMap.SampleLevel(SampleShadow, suv, 0.0).r;
		lit += (zReceiver <= d + bias) ? 1.0 : 0.0;
	}
	return lit * (1.0 / 16.0);
}

float ComputeShadowPCSS(float4 ShadowCoord, float3 Normal)
{
	float outVis = 1.0;
	const float w = ShadowCoord.w;
	if (abs(w) >= 1e-6)
	{
		const float3 proj = ShadowCoord.xyz / w;
		if (proj.z > 0.0 && proj.z < 1.0)
		{
			const float2 uv = proj.xy * float2(0.5, -0.5) + float2(0.5, 0.5);
			if (all(uv >= float2(0.0, 0.0)) && all(uv <= float2(1.0, 1.0)))
			{
				const float zR = clamp(proj.z, 0.0, 1.0);
				const float bias = ShadowDepthBiasPCSS(Normal);
				float3 Lsun = normalize(GetMainLight().Direction);
				float grazingSun = saturate(1.0 - abs(Lsun.y));

				float sumBlocker = 0.0;
				float cnt = 0.0;
				[unroll]
				for (int j = 0; j < 16; ++j)
				{
					float2 suv = uv + kPoissonDisk16[j] * kPCSSBlockerSearchRadiusUV;
					suv = clamp(suv, float2(1e-4, 1e-4), float2(1.0 - 1e-4, 1.0 - 1e-4));
					float d = ShadowMap.SampleLevel(SampleShadow, suv, 0.0).r;
					if (d < zR - bias)
					{
						sumBlocker += d;
						cnt += 1.0;
					}
				}

				float filterUV = kPCSSMinFilterRadiusUV + grazingSun * 0.00095;
				if (cnt >= 1.0)
				{
					float avgB = sumBlocker / cnt;
					float pen = saturate((zR - avgB) * kPCSSPenumbraMul);
					filterUV = lerp(kPCSSMinFilterRadiusUV, kPCSSMaxFilterRadiusUV, pen);
				}

				outVis = PCF_ShadowR32(uv, zR, filterUV, bias);
			}
		}
	}
	return outVis;
}

/** Vertical atlas: cascade `ci` occupies v in [ci/N, (ci+1)/N). `uvTile01` is x,y within the cascade tile (same mapping as ComputeShadowPCSS local uv). */
float ComputeShadowPCSSCascadeTile(float2 uvTile01, float cascadeIdx, float invN, float zReceiver, float3 Normal)
{
	float outVis = 1.0;
	if (zReceiver > 0.0 && zReceiver < 1.0 && all(uvTile01 >= float2(0.0, 0.0)) && all(uvTile01 <= float2(1.0, 1.0)))
	{
		float2 uvAtlas = float2(uvTile01.x, (cascadeIdx + uvTile01.y) * invN);
		const float bias = ShadowDepthBiasPCSS(Normal);
		float3 Lsun = normalize(GetMainLight().Direction);
		float grazingSun = saturate(1.0 - abs(Lsun.y));

		float sumBlocker = 0.0;
		float cnt = 0.0;
		[unroll]
		for (int j = 0; j < 16; ++j)
		{
			float2 ofs = float2(kPoissonDisk16[j].x, kPoissonDisk16[j].y * invN) * kPCSSBlockerSearchRadiusUV;
			float2 suv = uvAtlas + ofs;
			suv = clamp(suv, float2(1e-4, 1e-4), float2(1.0 - 1e-4, 1.0 - 1e-4));
			float d = ShadowMap.SampleLevel(SampleShadow, suv, 0.0).r;
			if (d < zReceiver - bias)
			{
				sumBlocker += d;
				cnt += 1.0;
			}
		}

		float filterUV = kPCSSMinFilterRadiusUV + grazingSun * 0.00095;
		if (cnt >= 1.0)
		{
			float avgB = sumBlocker / cnt;
			float pen = saturate((zReceiver - avgB) * kPCSSPenumbraMul);
			filterUV = lerp(kPCSSMinFilterRadiusUV, kPCSSMaxFilterRadiusUV, pen);
		}

		float2 filterOfsScale = float2(1.0, invN);
		float lit = 0.0;
		[unroll]
		for (int i = 0; i < 16; ++i)
		{
			float2 suv = uvAtlas + kPoissonDisk16[i] * filterOfsScale * filterUV;
			suv = clamp(suv, float2(1e-4, 1e-4), float2(1.0 - 1e-4, 1.0 - 1e-4));
			float d = ShadowMap.SampleLevel(SampleShadow, suv, 0.0).r;
			lit += (zReceiver <= d + bias) ? 1.0 : 0.0;
		}
		outVis = lit * (1.0 / 16.0);
	}
	return outVis;
}

#endif // MINIENGINE_SHADOW_PCSS_HLSL
