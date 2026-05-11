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
	float3x3 tbn = float3x3(Input.Tangent, Input.Binormal, Input.Normal);

	float3 n = getNormalTexture(Input);
	n = normalize(mul(n, tbn));

	return n * (bIsFontFacing ? -1 : 1);
}

/** glTF doubleSided: shading normal faces the camera (back faces flip N). Requires cbPerFrame / myPerFrame. */
float3 ShadeNormalDoubleSided(float3 n, float3 worldPos)
{
	if ((myMaterial.MaterialShaderFlags & kMatShaderFlag_DoubleSidedShading) != 0)
	{
		float3 v = normalize(myPerFrame.CameraPos.xyz - worldPos);
		if (dot(n, v) < 0.0)
			n = -n;
	}
	return n;
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
