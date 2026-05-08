// Spotlight depth (t11) + cbSpotShadow (b5). Include after ShadowPCSS.hlsl (uses kPoissonDisk16) and after SampleShadow is bound.

#ifndef MINIENGINE_SPOT_SHADOW_SAMPLING_HLSL
#define MINIENGINE_SPOT_SHADOW_SAMPLING_HLSL

Texture2D SpotShadowMap : register(t11);
cbuffer cbSpotShadow : register(b5)
{
	row_major matrix SpotLightViewProj;
	int SpotShadowEnabled;
	int SpotShadowLightIndex;
	uint2 _cbSpotPad0;
	uint4 _cbSpotPad1;
};

float SpotShadowReceiverBias(float3 normal, float3 LtowardLight)
{
	float3 n = normalize(normal);
	float3 L = normalize(LtowardLight);
	float NdotL = abs(dot(n, L));
	const float baseBias = 0.00035;
	const float slopeBias = 0.0011;
	return baseBias + slopeBias * (1.0 - NdotL);
}

float PCF_SpotShadowR32(float2 uvCenter, float zReceiver, float radiusUV, float bias)
{
	float lit = 0.0;
	[unroll]
	for (int i = 0; i < 16; ++i)
	{
		float2 suv = uvCenter + kPoissonDisk16[i] * radiusUV;
		suv = clamp(suv, float2(1e-4, 1e-4), float2(1.0 - 1e-4, 1.0 - 1e-4));
		float d = SpotShadowMap.SampleLevel(SampleShadow, suv, 0.0).r;
		lit += (zReceiver <= d + bias) ? 1.0 : 0.0;
	}
	return lit * (1.0 / 16.0);
}

float SampleSpotShadowVisibility(float4 ShadowCoord, float3 Normal, float3 LtowardLight)
{
	if (SpotShadowEnabled == 0)
		return 1.0;
	const float w = ShadowCoord.w;
	if (abs(w) < 1e-6)
		return 1.0;
	const float3 proj = ShadowCoord.xyz / w;
	if (proj.z <= 0.0 || proj.z >= 1.0)
		return 1.0;
	const float2 uv = proj.xy * float2(0.5, -0.5) + float2(0.5, 0.5);
	if (any(uv < 0.0) || any(uv > 1.0))
		return 1.0;
	const float zR = clamp(proj.z, 0.0, 1.0);
	const float bias = SpotShadowReceiverBias(Normal, LtowardLight);
	return clamp(PCF_SpotShadowR32(uv, zR, 0.0022, bias), 0.0, 1.0);
}

#endif // MINIENGINE_SPOT_SHADOW_SAMPLING_HLSL
