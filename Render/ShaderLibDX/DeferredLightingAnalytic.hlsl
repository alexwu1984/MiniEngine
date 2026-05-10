// Analytic lights + GGX BRDF terms shared by fullscreen deferred lighting and forward translucency (UE4-style separate translucency).
#ifndef MINIENGINE_DEFERRED_LIGHTING_ANALYTIC_HLSL
#define MINIENGINE_DEFERRED_LIGHTING_ANALYTIC_HLSL

float3 Diffuse(MaterialInfo materialInfo)
{
	return materialInfo.diffuseColor / PI;
}

float3 SpecularReflection(MaterialInfo MaterialInfo, AngularInfo angularInfo)
{
	return MaterialInfo.reflectance0 + (MaterialInfo.reflectance90 - MaterialInfo.reflectance0) * pow(clamp(1.0 - angularInfo.VdotH, 0.0, 1.0), 5.0);
}

float VisibilityOcclusion(MaterialInfo MaterialInfo, AngularInfo AngularInfo)
{
	float NdotL = AngularInfo.NdotL;
	float NdotV = AngularInfo.NdotV;
	float alphaRoughnessSq = MaterialInfo.alphaRoughness * MaterialInfo.alphaRoughness;
	float GGXV = NdotL * sqrt(NdotV * NdotV * (1.0 - alphaRoughnessSq) + alphaRoughnessSq);
	float GGXL = NdotV * sqrt(NdotL * NdotL * (1.0 - alphaRoughnessSq) + alphaRoughnessSq);
	float GGX = GGXV + GGXL;
	float vis = 0.0;
	if (GGX > 0.0)
		vis = 0.5 / GGX;
	return vis;
}

float MicrofacetDistribution(MaterialInfo MaterialInfo, AngularInfo AngularInfo)
{
	float alphaRoughnessSq = MaterialInfo.alphaRoughness * MaterialInfo.alphaRoughness;
	float f = (AngularInfo.NdotH * alphaRoughnessSq - AngularInfo.NdotH) * AngularInfo.NdotH + 1.0;
	return alphaRoughnessSq / (PI * f * f + 0.000001f);
}

float3 GetPointShade(float3 PointToLight, MaterialInfo MaterialInfo, float3 Normal, float3 View)
{
	AngularInfo angularInfo = GetAngularInfo(PointToLight, Normal, View);
	float3 shade = float3(0.0, 0.0, 0.0);
	if (angularInfo.NdotL > 0.0 || angularInfo.NdotV > 0.0)
	{
		float3 F = SpecularReflection(MaterialInfo, angularInfo);
		float Vis = VisibilityOcclusion(MaterialInfo, angularInfo);
		float D = MicrofacetDistribution(MaterialInfo, angularInfo);
		float3 diffuseContrib = (1.0 - F) * Diffuse(MaterialInfo);
		float3 specContrib = F * Vis * D;
		shade = angularInfo.NdotL * (diffuseContrib + specContrib);
	}
	return shade;
}

float3 ApplyDirectionalLightDeferred(float3 worldPos, Light light, MaterialInfo materialInfo, float3 normal, float3 view)
{
	float3 shade = GetPointShade(light.Direction, materialInfo, normal, view);
	float visibility = 1.0f;
	if (light.ShadowMapIndex >= 0)
		visibility = clamp(DirectionalShadowVisibility(worldPos, normal), 0.0, 1.0);
	return light.Intensity * light.Color * shade * visibility;
}

float3 ApplyPointLight(Light light, MaterialInfo materialInfo, float3 normal, float3 worldPos, float3 view, int lightIndex)
{
	float3 pointToLight = light.Position - worldPos;
	float distance = length(pointToLight);
	float attenuation = GetRangeAttenuation(light.Range, distance);
	float3 shade = GetPointShade(pointToLight, materialInfo, normal, view);
	float vis = 1.0;
	if (light.ShadowMapIndex == kPointLightCubeShadowMapIndex && PointShadowEnabled != 0 && lightIndex == PointShadowLightIndex)
		vis = SamplePointShadowCubeVisibility(worldPos, light.Position, light.Range);
	return attenuation * light.Intensity * light.Color * shade * vis;
}

float3 ApplySpotLight(Light light, MaterialInfo materialInfo, float3 normal, float3 worldPos, float3 view, int lightIndex)
{
	float3 pointToLight = light.Position - worldPos;
	float distance = length(pointToLight);
	float rangeAttenuation = GetRangeAttenuation(light.Range, distance);
	float spotAttenuation = GetSpotAttenuation(pointToLight, -light.Direction, light.OuterConeCos, light.InnerConeCos);
	float3 shade = GetPointShade(pointToLight, materialInfo, normal, view);
	float vis = 1.0;
	if (light.ShadowMapIndex == kSpotLightShadowMapIndex && SpotShadowEnabled != 0 && lightIndex == SpotShadowLightIndex)
	{
		float4 clip = mul(float4(worldPos, 1.0), SpotLightViewProj);
		float3 L = pointToLight / max(distance, 1e-5);
		vis = SampleSpotShadowVisibility(clip, normal, L);
	}
	return rangeAttenuation * spotAttenuation * light.Intensity * light.Color * shade * vis;
}

#endif // MINIENGINE_DEFERRED_LIGHTING_ANALYTIC_HLSL
