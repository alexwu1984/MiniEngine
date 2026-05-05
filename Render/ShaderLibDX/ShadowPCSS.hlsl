// PCSS for directional shadow map (R32 linear/hardware depth in [0,1], same space as LightViewProj clip z/w).
// Expects Texture2D ShadowMap, SamplerState SampleShadow, GetMainLight() from PerFrameStruct.hlsl.

#ifndef MINIENGINE_SHADOW_PCSS_HLSL
#define MINIENGINE_SHADOW_PCSS_HLSL

static const float kPCSSBlockerSearchRadiusUV = 0.010;
static const float kPCSSMinFilterRadiusUV = 0.00022;
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
	const float baseBias = 0.00025;
	const float slopeBias = 0.0009;
	return baseBias + slopeBias * (1.0 - NdotL);
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
	float vis = 1.0;
	const float w = ShadowCoord.w;
	if (abs(w) < 1e-6)
		return vis;
	const float3 proj = ShadowCoord.xyz / w;
	if (proj.z <= 0.0 || proj.z >= 1.0)
		return vis;
	const float2 uv = proj.xy * float2(0.5, -0.5) + float2(0.5, 0.5);
	if (any(uv < 0.0) || any(uv > 1.0))
		return vis;

	const float zR = clamp(proj.z, 0.0, 1.0);
	const float bias = ShadowDepthBiasPCSS(Normal);

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

	float filterUV = kPCSSMinFilterRadiusUV;
	if (cnt >= 1.0)
	{
		float avgB = sumBlocker / cnt;
		float pen = saturate((zR - avgB) * kPCSSPenumbraMul);
		filterUV = lerp(kPCSSMinFilterRadiusUV, kPCSSMaxFilterRadiusUV, pen);
	}

	return PCF_ShadowR32(uv, zR, filterUV, bias);
}

#endif // MINIENGINE_SHADOW_PCSS_HLSL
