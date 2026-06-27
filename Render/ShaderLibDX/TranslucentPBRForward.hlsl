// Forward-shaded translucent PBR after deferred lighting (UE4-style separate translucency).
// Material t0-t4 + cbPerMaterial b6; IBL/shadows t5-t8, t10-t12 + b4/b5/b7 (DeferredLightingPass::BindFurForwardSharedSRVs).
// t9 BackgroundSceneColor: same-frame copy of lit SceneColor (SceneColorWithSSR) for KHR_materials_transmission.
#define MINIENGINE_DEFERRED_LIGHTING_SKIP_HAIR 1

#include "PBRMaterialSampling.hlsl"

TextureCube IrradianceTex : register(t5);
Texture2D BrdfLut : register(t6);
TextureCube PrefilterCubeMap : register(t7);
Texture2D ShadowMap : register(t8);
Texture2D BackgroundSceneColor : register(t9);
TextureCube PointShadowCube : register(t10);
Texture2D GroundEnvLatLong : register(t12);

StructuredBuffer<Light> _SceneLights : register(t13);

#include "ClusterLightLookup.hlsl"

SamplerState SampleShadow : register(s1);
SamplerComparisonState ShadowCompareSampler : register(s2);

#include "DirectionalShadowCB.hlsl"
#include "TranslucentShadowLite.hlsl"

cbuffer cbPointShadow : register(b4)
{
	row_major matrix PointFaceVP[6];
	float4 PointShadowLightPosRange;
	int PointShadowEnabled;
	int PointShadowLightIndex;
	uint2 PointShadowPad;
};

#include "SpotShadowSampling.hlsl"
#include "DeferredLightingShared.hlsl"
#include "DeferredLightingAnalytic.hlsl"

struct PS_OUTPUT_TRANSLUCENT_FWD
{
	float4 SceneColor : SV_Target0;
	float4 Velocity : SV_Target1;
};

float2 NdcFromPixel(float2 pixelXY)
{
	float2 uv = (pixelXY + 0.5) * myPerFrame.InvScreenResolution;
	return float2(uv.x * 2.0 - 1.0, 1.0 - uv.y * 2.0);
}

float3 SampleTransmissionBackgroundAtNDC(float2 ndcXY)
{
	float2 uv = float2(ndcXY.x * 0.5 + 0.5, -ndcXY.y * 0.5 + 0.5);
	uv = clamp(uv, float2(0.0, 0.0), float2(1.0, 1.0));
	return BackgroundSceneColor.Sample(SampleLinear, uv).rgb;
}

float3 GetVolumeTransmissionRay(float3 n, float3 v, float thickness, float ior, float3 modelScale)
{
	float3 refractionVector = refract(-v, normalize(n), 1.0 / max(ior, 1.0));
	if (dot(refractionVector, refractionVector) < 1e-8)
		return float3(0.0, 0.0, 0.0);
	return normalize(refractionVector) * thickness * modelScale;
}

float3 VolumeAttenuation(float transmissionDistance, float3 attenuationColor, float attenuationDistance)
{
	float3 attCoeff = -log(max(attenuationColor, 1e-6)) / max(attenuationDistance, 1e-4);
	return exp(-attCoeff * max(transmissionDistance, 0.0));
}

float2 ComputeTransmissionSampleNDC(float3 worldPos, float3 normal, float3 view, float thickness, float ior, float3 modelScale, float2 ndcSurf)
{
	float3 transmissionRay = GetVolumeTransmissionRay(normal, view, thickness, ior, modelScale);
	float avgScale = (modelScale.x + modelScale.y + modelScale.z) / 3.0;
	float dist = max(length(transmissionRay), thickness * avgScale);

	float2 ndcSample = ndcSurf;
	if (dot(transmissionRay, transmissionRay) >= 1e-8)
	{
		float4 clipRefr = mul(float4(worldPos + transmissionRay, 1.0), myPerFrame.CameraCurrViewProj);
		float wR = max(abs(clipRefr.w), 1e-5);
		ndcSample = clipRefr.xy / wR;
	}

	// View-space parallax: volume exit alone often maps to same NDC on curved glass (no visible bend).
	float4 viewPos = mul(float4(worldPos, 1.0), myPerFrame.CameraWorldToView);
	float viewZ = max(abs(viewPos.z), 0.05);
	float3 Nview = normalize(mul(float4(normal, 0.0), myPerFrame.CameraWorldToView).xyz);
	float3 Vview = mul(float4(view, 0.0), myPerFrame.CameraWorldToView).xyz;
	float vLen = length(Vview);
	float3 VviewN = (vLen > 1e-5) ? (Vview / vLen) : float3(0.0, 0.0, 1.0);
	float3 Rview = refract(-VviewN, Nview, 1.0 / max(ior, 1.0));
	if (dot(Rview, Rview) >= 1e-8)
	{
		float2 ndcParallax = (Rview.xy / viewZ) * dist;
		ndcSample = ndcSurf + (ndcSample - ndcSurf) + ndcParallax;
	}

	return clamp(ndcSample, float2(-1.0, -1.0), float2(1.0, 1.0));
}

float3 SampleTransmissionBackgroundDispersed(float3 worldPos, float3 normal, float3 view, float thickness, float ior, float3 modelScale, float2 ndcSurf, float dispersion)
{
	if (dispersion <= 1e-4)
	{
		float2 ndc = ComputeTransmissionSampleNDC(worldPos, normal, view, thickness, ior, modelScale, ndcSurf);
		return SampleTransmissionBackgroundAtNDC(ndc);
	}

	float halfSpread = (ior - 1.0) * 0.025 * dispersion;
	float3 iors = float3(ior - halfSpread, ior, ior + halfSpread);
	float3 outRgb = float3(0.0, 0.0, 0.0);
	[unroll]
	for (int i = 0; i < 3; ++i)
	{
		float2 ndc = ComputeTransmissionSampleNDC(worldPos, normal, view, thickness, iors[i], modelScale, ndcSurf);
		float3 samp = SampleTransmissionBackgroundAtNDC(ndc);
		outRgb[i] = samp[i];
	}
	return outRgb;
}

float3 GetObjectModelScale()
{
	float3 sx = myPerObject_u_mCurrWorld[0].xyz;
	float3 sy = myPerObject_u_mCurrWorld[1].xyz;
	float3 sz = myPerObject_u_mCurrWorld[2].xyz;
	return float3(length(sx), length(sy), length(sz));
}

// glTF Sample Viewer / three.js EnvironmentBRDF (matches split-sum F weighting).
float3 EnvironmentBRDF(float3 n, float3 v, float3 specularColor, float3 specularF90, float perceptualRoughness)
{
	float NdotV = saturate(dot(n, v));
	float2 brdfUV = clamp(float2(NdotV, perceptualRoughness), float2(0.0, 0.0), float2(1.0, 1.0));
	float2 brdf = BrdfLut.Sample(SampleLinear, brdfUV).rg;
	return specularColor * brdf.x + specularF90 * brdf.y;
}

float3 SampleTransmissionBackgroundRefracted(float3 worldPos, float3 normal, float3 view, float thickness, float ior, float3 modelScale, float4 pixelPos, float dispersion)
{
	float2 ndcSurf = NdcFromPixel(pixelPos.xy);
	return SampleTransmissionBackgroundDispersed(worldPos, normal, view, thickness, ior, modelScale, ndcSurf, dispersion);
}

PS_OUTPUT_TRANSLUCENT_FWD MainPS_TranslucentForward(VS_OUTPUT_SCENE Input, float4 pixelPos : SV_Position)
{
	PS_OUTPUT_TRANSLUCENT_FWD Out;
	const float T = saturate(myMaterial.TransmissionFactor);
	const bool bTransmission = (T > 1e-3)
		|| ((myMaterial.MaterialShaderFlags & kMatShaderFlag_Transmission) != 0);
	Out.Velocity = bTransmission ? float4(0.0, 0.0, 0.0, 0.0)
		: float4(Calculate3DVelocity(Input.svCurrPosition, Input.svPrevPosition), 0.0);

	float3 worldPos = Input.WorldPos;
	float3 normal = ShadeNormalDoubleSided(getPixelNormal(Input), Input.WorldPos);

	float alpha;
	float perceptualRoughness;
	float metallic;
	float3 diffuseColor;
	float3 specularColor;
	GetPBRParams(Input, diffuseColor, specularColor, perceptualRoughness, metallic, alpha);

	if (bTransmission)
	{
		perceptualRoughness = max(perceptualRoughness, 0.0525);
		// KHR_materials_ior: F0 from index of refraction (DragonDispersion ior≈1.75).
		const float ior = max(myMaterial.MaterialIor, 1.0);
		const float f0 = pow(max((ior - 1.0) / (ior + 1.0), 0.0), 2.0);
		const float3 f0Old = float3(0.04, 0.04, 0.04);
		const float3 f0New = float3(f0, f0, f0);
		diffuseColor *= (1.0 - f0New) / max(1.0 - f0Old, 1e-4);
		specularColor = f0New;
	}

	if (myMaterial.AlphaMask != 0)
		clip(alpha - myMaterial.AlphaCutoff);

	if (myPerFrame.bUnlit != 0)
	{
		float4 bc = AlbedoMap.Sample(SampleLinear, Input.UV0);
		float3 albedoLin = sRGBToLinear(bc.rgb);
		float3 emLin = sRGBToLinear(EmissMap.Sample(SampleLinear, Input.UV0).rgb);
		Out.SceneColor = float4(albedoLin + emLin, bc.a);
		return Out;
	}

	MaterialInfo materialInfo;
	if (bTransmission)
		BuildMaterialInfoFromPBRColors(diffuseColor, specularColor, metallic, perceptualRoughness, materialInfo);
	else
	{
		float4 baseTex = AlbedoMap.Sample(SampleLinear, Input.UV0);
		float3 linearBase = sRGBToLinear(baseTex.rgb);
		DecodeMaterialFromGBuffer(linearBase, metallic, perceptualRoughness, materialInfo);
	}

	float aoRaw = 1.0;
	if (!bTransmission)
	{
		float4 aoSamp = AoMap.Sample(SampleLinear, Input.UV0);
		aoRaw = max(max(aoSamp.r, aoSamp.g), aoSamp.b);
	}
	const float aoDiffuse = max(aoRaw, 1e-4);
	const float aoSpec = max(aoRaw, 0.2);

	float3 viewVec = myPerFrame.CameraPos.xyz - worldPos;
	float vLen = length(viewVec);
	float3 view = (vLen > 1e-5) ? (viewVec / vLen) : float3(0.0, 0.0, 1.0);

	float3 color = float3(0, 0, 0);
	float3 directDiffuse = float3(0, 0, 0);
	float3 directSpecular = float3(0, 0, 0);

	const uint2 ClusterRange = _ClusterLightOffsetCount[ClusterIndexFromPixel(pixelPos)];
	[loop]
	for (uint slot = 0u; slot < ClusterRange.y; ++slot)
	{
		const uint i = _ClusterLightIndexList[ClusterRange.x + slot];
		Light light = _SceneLights[i];
		float3 shadeDiffuse = float3(0.0, 0.0, 0.0);
		float3 shadeSpecular = float3(0.0, 0.0, 0.0);
		float3 lightScale = float3(0.0, 0.0, 0.0);
		if (light.Type == LightType_Directional)
		{
			GetPointShadeSplit(light.Direction, materialInfo, normal, view, shadeDiffuse, shadeSpecular);
			float visibility = 1.0f;
			if (light.ShadowMapIndex >= 0)
				visibility = clamp(DirectionalShadowVisibility(worldPos, normal), 0.0, 1.0);
			lightScale = light.Intensity * light.Color * visibility;
		}
		else if (light.Type == LightType_Point)
		{
			float3 pointToLight = light.Position - worldPos;
			float distance = length(pointToLight);
			float attenuation = GetRangeAttenuation(light.Range, distance);
			GetPointShadeSplit(pointToLight, materialInfo, normal, view, shadeDiffuse, shadeSpecular);
			float vis = 1.0;
			if (light.ShadowMapIndex == kPointLightCubeShadowMapIndex && PointShadowEnabled != 0 && i == PointShadowLightIndex)
				vis = SamplePointShadowCubeVisibility(worldPos, light.Position, light.Range);
			lightScale = attenuation * light.Intensity * light.Color * vis;
		}
		else if (light.Type == LightType_Spot)
		{
			float3 pointToLight = light.Position - worldPos;
			float distance = length(pointToLight);
			float rangeAttenuation = GetRangeAttenuation(light.Range, distance);
			float spotAttenuation = GetSpotAttenuation(pointToLight, -light.Direction, light.OuterConeCos, light.InnerConeCos);
			GetPointShadeSplit(pointToLight, materialInfo, normal, view, shadeDiffuse, shadeSpecular);
			float vis = 1.0;
			if (light.ShadowMapIndex == kSpotLightShadowMapIndex && SpotShadowEnabled != 0 && i == SpotShadowLightIndex)
			{
				float4 clip = mul(float4(worldPos, 1.0), SpotLightViewProj);
				float3 L = pointToLight / max(distance, 1e-5);
				vis = SampleSpotShadowVisibility(clip, normal, L);
			}
			lightScale = rangeAttenuation * spotAttenuation * light.Intensity * light.Color * vis;
		}
		else
			continue;

		color += lightScale * (shadeDiffuse + shadeSpecular);
		directDiffuse += lightScale * shadeDiffuse;
		directSpecular += lightScale * shadeSpecular;
	}

	float3 iblDiffuse, iblSpecular;
	GetIBLContributionSplit(materialInfo, normal, view, iblDiffuse, iblSpecular);
	float NdotVao = saturate(dot(normal, view));
	float specOccPowBase = max(NdotVao + aoSpec - 0.0001, 1e-5);
	float specOcc = saturate(pow(specOccPowBase, exp2(-14.0 * perceptualRoughness - 0.62)) - 1.0 + aoSpec);

	const float coupleD = saturate(myPerFrame.IBLDirShadowCoupling.x);
	const float coupleS = saturate(myPerFrame.IBLDirShadowCoupling.y);
	float iblDiffScale = 1.0;
	float iblSpecScale = 1.0;
	if (coupleD > 0.0 || coupleS > 0.0)
	{
		const float dirVisIBL = PrimaryDirectionalShadowVisForIBL(worldPos, normal);
		iblDiffScale = lerp(1.0, dirVisIBL, coupleD);
		iblSpecScale = lerp(1.0, dirVisIBL, coupleS);
	}
	const float iblAoExp = max(myPerFrame.IBLDirShadowCoupling.z, 1e-3);
	const float aoForIblDiffuse = pow(max(aoDiffuse, 1e-4), iblAoExp);
	const float3 iblSpec = (iblSpecular * specOcc * iblSpecScale) * myPerFrame.IBLFactor;
	const float3 iblDiff = (iblDiffuse * aoForIblDiffuse * iblDiffScale) * myPerFrame.IBLFactor;
	float3 surface = color + iblDiff + iblSpec;

	float3 emiss = sRGBToLinear(EmissMap.Sample(SampleLinear, Input.UV0).rgb);
	surface += emiss;

	if (bTransmission)
	{
		float4 aoSamp = AoMap.Sample(SampleLinear, Input.UV0);
		float thicknessSample = max(aoSamp.g, aoSamp.r);
		float thickness = max(thicknessSample * myMaterial.ThicknessFactor, 1e-3);
		float3 modelScale = GetObjectModelScale();
		float avgScale = (modelScale.x + modelScale.y + modelScale.z) / 3.0;
		float ior = max(myMaterial.MaterialIor, 1.0);
		float dispersion = max(myMaterial.MaterialDispersion, 0.0);

		float3 transmissionRay = GetVolumeTransmissionRay(normal, view, thickness, ior, modelScale);
		float dist = max(length(transmissionRay), thickness * avgScale);

		float3 bg = SampleTransmissionBackgroundRefracted(worldPos, normal, view, thickness, ior, modelScale, pixelPos, dispersion);

		float3 baseLin = sRGBToLinear(AlbedoMap.Sample(SampleLinear, Input.UV0).rgb);
		float3 vol = VolumeAttenuation(dist, myMaterial.AttenuationColor, myMaterial.AttenuationDistance);
		// three.js: transmittance = baseColor * volumeAttenuation(length(ray))
		float3 transmittance = baseLin * vol;
		float3 attenuated = bg * transmittance;

		const float3 specularF90 = float3(1.0, 1.0, 1.0);
		const float3 F = EnvironmentBRDF(normal, view, materialInfo.specularColor, specularF90, perceptualRoughness);

		float3 outgoingDiffuse = directDiffuse + iblDiff;
		float3 transmittedDiffuse = attenuated * (1.0 - F);
		float3 totalDiffuse = lerp(outgoingDiffuse, transmittedDiffuse, T);

		float3 outgoingSpecular = directSpecular + iblSpec;

		Out.SceneColor = float4(totalDiffuse + outgoingSpecular + emiss, 1.0);
		return Out;
	}

	Out.SceneColor = float4(surface, alpha);
	return Out;
}
