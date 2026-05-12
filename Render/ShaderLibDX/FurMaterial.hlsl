// Forward shell pass only: VS is FurPass-VS.hlsl (furVertexFactory); PS here. Inner fill uses PBRMaterial.hlsl.
#include "FurPass-IO.hlsl"
#include "PerFrameStruct.hlsl"
#include "ShaderUtils.hlsl"
#include "HairShading.hlsl"

#if defined(RHI_BINDLESS)
Texture2D Fur_Material2D[2] : register(t0);
#define AlbedoMap Fur_Material2D[0]
#define NoiseMap  Fur_Material2D[1]
#else
Texture2D AlbedoMap : register(t0);
Texture2D NoiseMap : register(t1);
#endif
SamplerState SampleLinear : register(s0);

TextureCube IrradianceTex : register(t5);
Texture2D BrdfLut : register(t6);
TextureCube PrefilterCubeMap : register(t7);
Texture2D ShadowMap : register(t8);
TextureCube PointShadowCube : register(t10);
Texture2D GroundEnvLatLong : register(t12);
SamplerState SampleShadow : register(s1);
SamplerComparisonState ShadowCompareSampler : register(s2);

cbuffer cbPointShadow : register(b4)
{
	row_major matrix PointFaceVP[6];
	float4 PointShadowLightPosRange;
	int PointShadowEnabled;
	int PointShadowLightIndex;
	uint2 PointShadowPad;
};

#include "ShadowPCSS.hlsl"

cbuffer cbDirectionalShadow : register(b7)
{
	row_major matrix DirectionalShadowViewProj;
	float4 _PadDirectionalShadow;
};

#include "DirectionalShadow.hlsl"
#include "SpotShadowSampling.hlsl"
#include "DeferredLightingShared.hlsl"
#include "FurForwardAccumulate.hlsl"

struct PS_OUTPUT_FWD
{
	float4 Color : SV_Target0;
};

PS_OUTPUT_FWD MainPS(VS_OUTPUT_FUR Input)
{
	PS_OUTPUT_FWD Output;
	Output.Color = float4(0, 0, 0, 0);

	float3 BaseColor = sRGBToLinear(AlbedoMap.Sample(SampleLinear, Input.UV1).rgb);
	float3 n = normalize(Input.Normal);
	const float kRough = 0.92;

	float Noise = NoiseMap.Sample(SampleLinear, Input.UV0).r;
	float FurMask = 0.5;
	float Tming = 0.5;
	float shellT = Input.FurShellOffset;
	float Alpha = clamp((Noise * 2.0 - (shellT * shellT + (shellT * FurMask * 5.0))) * Tming, 0.0, 1.0);
	clip(Alpha - 0.001);

	float3 strandDir = normalize(Input.Tangent);

	float3 Vw = normalize(myPerFrame.CameraPos.xyz - Input.WorldPos);
	float3 geomN = n;
	float edge = saturate((1.0 - Alpha) * 3.0);
	float3 upW = float3(0.0, 1.0, 0.0);
	geomN = normalize(lerp(geomN, upW, edge * 0.42));

	float Occlusion = shellT * shellT + 0.04;
	float Fresnel = 1.0 - max(0.0, dot(n, Vw));
	float3 RimLight = float3(Fresnel * Occlusion, Fresnel * Occlusion, Fresnel * Occlusion);
	RimLight *= RimLight;
	RimLight *= 0.55 * Input.SH * BaseColor * FurAmbientStrength * FurLightExposure;

	float3 shellAlbedo = BaseColor * FurLightExposure;
	float3 lit = AccumulateFurForwardShading(Input.WorldPos, geomN, strandDir, Vw, shellAlbedo, kRough, Alpha);
	lit += RimLight;

	if (myPerFrame.bUnlit != 0)
		lit = shellAlbedo + RimLight;

	Output.Color = float4(lit, Alpha);
	return Output;
}
