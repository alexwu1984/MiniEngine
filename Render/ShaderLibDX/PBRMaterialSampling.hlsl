// Shared PBR surface sampling for deferred base pass + translucent forward pass (UE-style translucency).
// Include after ShaderUtils + PerFrameStruct; defines material textures t0–t4 and cbPerMaterial (b6).

#ifndef MINIENGINE_PBR_MATERIAL_SAMPLING_HLSL
#define MINIENGINE_PBR_MATERIAL_SAMPLING_HLSL

#include "ShaderUtils.hlsl"
#include "PerFrameStruct.hlsl"
#include "GLTFPbrPass-IO.hlsl"

#if defined(RHI_BINDLESS)
Texture2D PBR_Material2D[5] : register(t0);
#define AlbedoMap PBR_Material2D[0]
#define NormalMap PBR_Material2D[1]
#define Roughness_metallicMap PBR_Material2D[2]
#define EmissMap PBR_Material2D[3]
#define AoMap PBR_Material2D[4]
#else
Texture2D AlbedoMap : register(t0);
Texture2D NormalMap : register(t1);
Texture2D Roughness_metallicMap : register(t2);
Texture2D EmissMap : register(t3);
Texture2D AoMap : register(t4);
#endif
SamplerState SampleLinear : register(s0);

cbuffer cbPerMaterial : register(b6)
{
	MaterialPerFrame myMaterial;
};

float3 getNormalTexture(VS_OUTPUT_SCENE Input)
{
	float2 xy = 2.0 * NormalMap.SampleBias(SampleLinear, Input.UV0, myPerFrame.LodBias).rg - 1.0;
	float lenSq = dot(xy, xy);
	lenSq = min(lenSq, 1.0 - 1e-6);
	float z = sqrt(max(1.0 - lenSq, 0.0));
	return float3(xy, z);
}

float3 getPixelNormal(VS_OUTPUT_SCENE Input, bool bIsFontFacing = false)
{
#ifndef HAS_TANGENT
	float2 UV = Input.UV0;
	float3 pos_dx = ddx(Input.WorldPos);
	float3 pos_dy = ddy(Input.WorldPos);
	float3 tex_dx = ddx(float3(UV, 0.0));
	float3 tex_dy = ddy(float3(UV, 0.0));
	float denom = tex_dx.x * tex_dy.y - tex_dy.x * tex_dx.y;
	float3 tUnnorm = tex_dy.y * pos_dx - tex_dx.y * pos_dy;
	float3 ng = normalize(Input.Normal);
	float3 t;
	if (abs(denom) > 1e-7)
		t = tUnnorm / denom;
	else
	{
		float3 up = abs(ng.y) < 0.99 ? float3(0.0, 1.0, 0.0) : float3(1.0, 0.0, 0.0);
		t = normalize(cross(up, ng));
	}

	t = normalize(t - ng * dot(ng, t));
	float3 b = normalize(cross(ng, t));
	float3x3 tbn = float3x3(t, b, ng);
#else
	float3x3 tbn = float3x3(Input.Tangent, Input.Binormal, Input.Normal);
#endif

	float3 n = getNormalTexture(Input);
	n = normalize(mul(n, tbn));

	return n * (bIsFontFacing ? -1 : 1);
}

void GetPBRParams(VS_OUTPUT_SCENE Input, out float3 diffuseColor, out float3 specularColor, out float perceptualRoughness, out float metallic, out float alpha)
{
	alpha = 0.0;
	perceptualRoughness = 0.0;
	diffuseColor = float3(0.0, 0.0, 0.0);
	specularColor = float3(0.0, 0.0, 0.0);
	float3 f0 = float3(0.04, 0.04, 0.04);

	float4 baseColor = AlbedoMap.Sample(SampleLinear, Input.UV0);

	float4 mr = Roughness_metallicMap.Sample(SampleLinear, Input.UV0);
	perceptualRoughness = mr.g;
	metallic = mr.b;

	diffuseColor = baseColor.rgb * (float3(1.0, 1.0, 1.0) - f0) * (1.0 - metallic);
	specularColor = lerp(f0, baseColor.rgb, metallic);

	perceptualRoughness = clamp(perceptualRoughness, 0.0, 1.0);

	alpha = baseColor.a;
}

float3 Calculate3DVelocity(float4 CurrentVelocity, float4 PreVelocity)
{
	float Wc = CurrentVelocity.w != 0.0 ? CurrentVelocity.w : 1e-8;
	float Wp = PreVelocity.w != 0.0 ? PreVelocity.w : 1e-8;
	float2 ScreenPos = CurrentVelocity.xy / Wc - myPerFrame.TemporalAAJitter.xy;
	float2 PrevScreenPos = PreVelocity.xy / Wp - myPerFrame.TemporalAAJitter.zw;

	float DeviceZ = CurrentVelocity.z / Wc;
	float PrevDeviceZ = PreVelocity.z / Wp;

	float3 Velocity = float3(ScreenPos - PrevScreenPos, DeviceZ - PrevDeviceZ);
	return Velocity;
}

#endif // MINIENGINE_PBR_MATERIAL_SAMPLING_HLSL
