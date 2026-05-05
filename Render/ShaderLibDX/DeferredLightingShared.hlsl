// Shared split-sum IBL decode + hair strand analytic lights (directional/point/spot + cube shadow).
// Include after: ShaderUtils, PerFrameStruct, HairShading (optional; guarded), IrradianceTex/BrdfLut/PrefilterCubeMap (t5–t7),
// ShadowMap/PointShadowCube, SampleLinear/SampleShadow, ShadowPCSS.hlsl, cbPointShadow.

#ifndef MINIENGINE_DEFERRED_LIGHTING_SHARED_HLSL
#define MINIENGINE_DEFERRED_LIGHTING_SHARED_HLSL

#include "HairShading.hlsl"

static const int kPointLightCubeShadowMapIndex = 2;

struct MaterialInfo
{
	float perceptualRoughness;
	float3 reflectance0;
	float alphaRoughness;
	float3 diffuseColor;
	float3 reflectance90;
	float3 specularColor;
	float Metallic;
};

void DecodeMaterialFromGBuffer(float3 baseColor, float metallic, float perceptualRoughness, out MaterialInfo materialInfo)
{
	float3 f0 = float3(0.04, 0.04, 0.04);
	materialInfo.Metallic = metallic;
	materialInfo.perceptualRoughness = clamp(perceptualRoughness, 0.0, 1.0);
	materialInfo.alphaRoughness = materialInfo.perceptualRoughness * materialInfo.perceptualRoughness;
	materialInfo.diffuseColor = baseColor * (float3(1.0, 1.0, 1.0) - f0) * (1.0 - metallic);
	materialInfo.specularColor = lerp(f0, baseColor, metallic);
	// glTF: metal uses baseColor as F82 tint — near-black albedo zeros split-sum IBL specular. Keep a minimal conductor
	// response so environment/cubemap still reads on dark chrome / paint (matches common game/Tutorial look).
	const float specLum = dot(materialInfo.specularColor, float3(0.2126, 0.7152, 0.0722));
	if (metallic > 0.5 && specLum < 0.003)
		materialInfo.specularColor = max(materialInfo.specularColor, f0);
	float reflectance = max(max(materialInfo.specularColor.r, materialInfo.specularColor.g), materialInfo.specularColor.b);
	materialInfo.reflectance0 = materialInfo.specularColor;
	materialInfo.reflectance90 = float3(1.0, 1.0, 1.0) * clamp(reflectance * 50.0, 0.0, 1.0);
}

void GetIBLContributionSplit(MaterialInfo MaterialInfo, float3 n, float3 v, out float3 outDiffuseIBL, out float3 outSpecularIBL)
{
	float NdotV = clamp(dot(n, v), 0.0, 1.0);
	float u_MipCount = myPerFrame.IBLMIpCount;
	float maxMipIndex = max(u_MipCount - 1.0, 0.0);
	float lod = clamp(MaterialInfo.perceptualRoughness * maxMipIndex, 0.0, maxMipIndex);
	float3 reflection = normalize(reflect(-v, n));
	reflection = mul(float4(reflection, 1.0), myPerFrame.RotateIBL).xyz;
	float2 brdfUV = clamp(float2(NdotV, MaterialInfo.perceptualRoughness), float2(0.0, 0.0), float2(1.0, 1.0));
	float2 BRDF = BrdfLut.Sample(SampleLinear, brdfUV).rg;
	float3 DiffuseLight = IrradianceTex.Sample(SampleLinear, n).rgb;
	float3 SpecularLight = PrefilterCubeMap.SampleLevel(SampleLinear, reflection, lod).rgb;
	outDiffuseIBL = DiffuseLight * MaterialInfo.diffuseColor;
	outSpecularIBL = SpecularLight * (MaterialInfo.specularColor * BRDF.x + BRDF.y);
}

float GetRangeAttenuation(float Range, float Distance)
{
	float att = 1.0;
	if (Range > 0.0)
	{
		const float denom = max(Range, 1e-5);
		const float t = saturate(Distance / denom);
		att = max(1.0 - t, 0.0);
	}
	return att;
}

float GetSpotAttenuation(float3 PointToLight, float3 SpotDirection, float OuterConeCos, float InnerConeCos)
{
	float att = 0.0;
	float actualCos = dot(normalize(SpotDirection), normalize(-PointToLight));
	if (actualCos > OuterConeCos)
	{
		if (actualCos < InnerConeCos)
			att = smoothstep(OuterConeCos, InnerConeCos, actualCos);
		else
			att = 1.0;
	}
	return att;
}

float ComputeShadow(float4 ShadowCoord, float3 Normal)
{
	return clamp(ComputeShadowPCSS(ShadowCoord, Normal), 0.0, 1.0);
}

int PointShadowCubeFaceIndex(float3 dirW)
{
	float3 a = abs(dirW);
	if (a.x >= a.y && a.x >= a.z)
		return dirW.x > 0.0 ? 0 : 1;
	if (a.y >= a.z)
		return dirW.y > 0.0 ? 2 : 3;
	return dirW.z > 0.0 ? 4 : 5;
}

float SamplePointShadowCubeVisibility(float3 worldPos, float3 lightPos, float lightRange)
{
	if (PointShadowEnabled == 0)
		return 1.0;
	float3 toFrag = worldPos - lightPos;
	float dist = length(toFrag);
	if (dist >= lightRange - 1e-3)
		return 1.0;
	float3 dir = toFrag / max(dist, 1e-5);
	int face = PointShadowCubeFaceIndex(dir);
	float4 clip = mul(float4(worldPos, 1.0), PointFaceVP[face]);
	float zR = clip.z / max(clip.w, 1e-6);
	float zMap = PointShadowCube.SampleLevel(SampleShadow, dir, 0).r;
	float bias = 0.002;
	return (zR <= zMap + bias) ? 1.0 : 0.0;
}

float DirectionalShadowHair(float4 lightClipPos, float3 geomN)
{
	float visibility = 1.0f;
	if (IsEnableShadow())
		visibility = clamp(ComputeShadow(lightClipPos, geomN), 0.0, 1.0);
	return visibility;
}

// Fur shells: GBuffer normal is SrcAlpha-blended vs floor; geom N·L collapses on contact while sky pixels often
// take the depth>=1 early-out (unlit baseColor only). Soft minimum + wrap on translucent edges matches sky-side read.
float HairShellNdotL(float3 geomN, float3 L, float coverageAlpha)
{
	float n = saturate(dot(geomN, L));
	float e = saturate((1.0 - coverageAlpha) * 4.0);
	n = max(n, e * 0.38);
	return lerp(n, saturate(n * 0.62 + 0.32), e * 0.55);
}

float3 ApplyDirectionalLightHair(float4 lightClipPos, Light light, float3 baseColor, float perceptualRoughness, float ao,
	float3 strandT, float3 geomN, float3 view, float coverageAlpha)
{
	float3 L = normalize(light.Direction);
	float NdotL = HairShellNdotL(geomN, L, coverageAlpha);
	float3 diffKK, specKK;
	KajiyaKayTerms(strandT, L, view, perceptualRoughness, baseColor, diffKK, specKK);
	float visibility = DirectionalShadowHair(lightClipPos, geomN);
	float specMask = saturate(NdotL * 0.55 + 0.38);
	return light.Intensity * light.Color * (diffKK * NdotL + specKK * specMask) * ao * visibility;
}

float3 ApplyPointLightHair(Light light, float3 baseColor, float perceptualRoughness, float ao,
	float3 strandT, float3 geomN, float3 worldPos, float3 view, int lightIndex, float coverageAlpha)
{
	float3 pointToLight = light.Position - worldPos;
	float distance = length(pointToLight);
	float3 L = pointToLight / max(distance, 1e-5);
	float attenuation = GetRangeAttenuation(light.Range, distance);
	float NdotL = HairShellNdotL(geomN, L, coverageAlpha);
	float3 diffKK, specKK;
	KajiyaKayTerms(strandT, L, view, perceptualRoughness, baseColor, diffKK, specKK);
	float vis = 1.0;
	if (light.ShadowMapIndex == kPointLightCubeShadowMapIndex && PointShadowEnabled != 0 && lightIndex == PointShadowLightIndex)
		vis = SamplePointShadowCubeVisibility(worldPos, light.Position, light.Range);
	float specMask = saturate(NdotL * 0.55 + 0.38);
	return attenuation * light.Intensity * light.Color * (diffKK * NdotL + specKK * specMask) * ao * vis;
}

float3 ApplySpotLightHair(Light light, float3 baseColor, float perceptualRoughness, float ao,
	float3 strandT, float3 geomN, float3 worldPos, float3 view, float coverageAlpha)
{
	float3 pointToLight = light.Position - worldPos;
	float distance = length(pointToLight);
	float rangeAttenuation = GetRangeAttenuation(light.Range, distance);
	float spotAttenuation = GetSpotAttenuation(pointToLight, -light.Direction, light.OuterConeCos, light.InnerConeCos);
	float3 L = pointToLight / max(distance, 1e-5);
	float NdotL = HairShellNdotL(geomN, L, coverageAlpha);
	float3 diffKK, specKK;
	KajiyaKayTerms(strandT, L, view, perceptualRoughness, baseColor, diffKK, specKK);
	float specMask = saturate(NdotL * 0.55 + 0.38);
	return rangeAttenuation * spotAttenuation * light.Intensity * light.Color * (diffKK * NdotL + specKK * specMask) * ao;
}

#endif // MINIENGINE_DEFERRED_LIGHTING_SHARED_HLSL
