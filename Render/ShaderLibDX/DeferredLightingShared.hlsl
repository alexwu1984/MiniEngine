// Shared split-sum IBL decode + hair strand analytic lights (directional/point/spot + cube shadow).
// Include after: ShaderUtils, PerFrameStruct, HairShading (optional; guarded), IrradianceTex/BrdfLut/PrefilterCubeMap (t5–t7),
// ShadowMap/PointShadowCube, GroundEnvLatLong (t12, optional split hemi), SampleLinear/SampleShadow, ShadowPCSS.hlsl, cbPointShadow.

#ifndef MINIENGINE_DEFERRED_LIGHTING_SHARED_HLSL
#define MINIENGINE_DEFERRED_LIGHTING_SHARED_HLSL

#include "HairShading.hlsl"

static const int kPointLightCubeShadowMapIndex = 2;
static const int kSpotLightShadowMapIndex = 3;

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

float2 DirectionToLatLongUV(float3 dir)
{
	float3 v = normalize(dir);
	float2 uv = float2(atan2(v.z, v.x), asin(clamp(v.y, -1.0, 1.0)));
	static const float2 invAtan = float2(0.159154943, -0.318309886);
	return saturate(uv * invAtan + 0.5);
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

	float3 DiffuseLight;
	float3 SpecularLight;

	if (myPerFrame.SplitHemisphereIBL != 0)
	{
		float pwr = max(myPerFrame.HemiIBLBlendPower, 0.08);
		float tN = saturate(n.y * 0.5 + 0.5);
		float wSkyN = pow(tN, pwr);
		float wGrN = pow(1.0 - tN, pwr);
		float sN = max(wSkyN + wGrN, 1e-4);
		wSkyN /= sN;
		wGrN /= sN;

		float3 irrSky = IrradianceTex.Sample(SampleLinear, n).rgb;
		float3 irrGr = GroundEnvLatLong.SampleLevel(SampleLinear, DirectionToLatLongUV(n), 0).rgb * myPerFrame.GroundIBLIntensity;
		DiffuseLight = irrSky * wSkyN + irrGr * wGrN;

		float tR = saturate(reflection.y * 0.5 + 0.5);
		float wSkyR = pow(tR, pwr);
		float wGrR = pow(1.0 - tR, pwr);
		float sR = max(wSkyR + wGrR, 1e-4);
		wSkyR /= sR;
		wGrR /= sR;
		float specLodGr = clamp(lod * 0.85, 0.0, maxMipIndex);
		float3 specSky = PrefilterCubeMap.SampleLevel(SampleLinear, reflection, lod).rgb;
		float3 specGr = GroundEnvLatLong.SampleLevel(SampleLinear, DirectionToLatLongUV(reflection), specLodGr).rgb * myPerFrame.GroundIBLIntensity;
		SpecularLight = specSky * wSkyR + specGr * wGrR;
	}
	else
	{
		DiffuseLight = IrradianceTex.Sample(SampleLinear, n).rgb;
		SpecularLight = PrefilterCubeMap.SampleLevel(SampleLinear, reflection, lod).rgb;
	}

	outDiffuseIBL = DiffuseLight * MaterialInfo.diffuseColor;
	outSpecularIBL = SpecularLight * (MaterialInfo.specularColor * BRDF.x + BRDF.y);
}

// Inverse-squared attenuation aligned with UE4 punctual radial path (see UE DeferredLightingCommon.usf):
//   DistanceAttenuation = rcp(DistanceSqr + 1) * Square(saturate(1 - Square(DistanceSqr * Square(InvRadius))));
// Unlimited range (MiniEngine convention Range < 0): skip radius mask — softened inverse-square only (no hard rim).
float GetRangeAttenuation(float Range, float Distance)
{
	float d = max(Distance, 0.0);
	float d2 = d * d;
	float distanceAttenuation = rcp(max(d2, 1e-8) + 1.0);
	if (Range < 0.0)
		return distanceAttenuation;
	float r = max(Range, 1e-4);
	float invR = 1.0 / r;
	float dd = max(d2 * (invR * invR), 0.0);
	float dd4 = dd * dd;
	float lightRadiusMaskSq = saturate(1.0 - dd4);
	float lightRadiusMask = lightRadiusMaskSq * lightRadiusMaskSq;
	return distanceAttenuation * lightRadiusMask;
}

float GetSpotAttenuation(float3 PointToLight, float3 SpotDirection, float OuterConeCos, float InnerConeCos)
{
	// KHR half-angles: inner < outer (degrees) => InnerConeCos > OuterConeCos. Some paths may swap; normalize order.
	float lo = min(OuterConeCos, InnerConeCos);
	float hi = max(OuterConeCos, InnerConeCos);
	float3 ax = normalize(SpotDirection);
	float3 toSurf = normalize(-PointToLight);
	float cosTheta = dot(ax, toSurf);
	if (hi - lo < 1e-5)
		return cosTheta >= lo - 1e-5 ? 1.0 : 0.0;
	return saturate(smoothstep(lo, hi, cosTheta));
}

float ComputeShadow(float4 ShadowCoord, float3 Normal)
{
	return clamp(ComputeShadowPCSS(ShadowCoord, Normal), 0.0, 1.0);
}

// Hair/fur: shadow map is rendered from the base mesh (no cbPerFur bound in shadow pass → FurOffset=0).
// Shell vertices sit in front of that depth in light space; PCSS blocker search + tight bias yields high-frequency
// broken stripes. Use stable PCF + stronger bias (extra from shell coverage).
float ComputeShadowHair(float4 ShadowCoord, float3 Normal, float coverageAlpha)
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
	float bias = ShadowDepthBiasPCSS(Normal);
	// Pull receivers off the occluder depth from the inner shell; wispy tips need more slack.
	bias += 0.00115 + saturate(1.0 - coverageAlpha) * 0.00135;
	const float fixedPcfRadius = 0.0020;
	return PCF_ShadowR32(uv, zR, fixedPcfRadius, bias);
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

float DirectionalShadowHair(float4 lightClipPos, float3 geomN, float coverageAlpha, bool bDirectionalShadow)
{
	float visibility = 1.0f;
	if (bDirectionalShadow)
		visibility = clamp(ComputeShadowHair(lightClipPos, geomN, coverageAlpha), 0.0, 1.0);
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
	float visibility = DirectionalShadowHair(lightClipPos, geomN, coverageAlpha, light.ShadowMapIndex >= 0);
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
	float3 strandT, float3 geomN, float3 worldPos, float3 view, int lightIndex, float coverageAlpha)
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
	float vis = 1.0;
	if (light.ShadowMapIndex == kSpotLightShadowMapIndex && SpotShadowEnabled != 0 && lightIndex == SpotShadowLightIndex)
	{
		float4 clip = mul(float4(worldPos, 1.0), SpotLightViewProj);
		vis = SampleSpotShadowVisibility(clip, geomN, L);
	}
	return rangeAttenuation * spotAttenuation * light.Intensity * light.Color * (diffKK * NdotL + specKK * specMask) * ao * vis;
}

#endif // MINIENGINE_DEFERRED_LIGHTING_SHARED_HLSL
