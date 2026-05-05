// Fur forward accumulate: include after DeferredLightingShared (MaterialInfo, lights, IBL split).

#ifndef FUR_FORWARD_ACCUMULATE_HLSL
#define FUR_FORWARD_ACCUMULATE_HLSL

float3 AccumulateFurForwardShading(float3 worldPos, float3 geomN, float3 strandT, float3 view, float3 baseColor,
	float perceptualRoughness, float coverageAlpha)
{
	const float aoDiffuse = 1.0;
	const float aoSpec = max(1.0, 0.2);
	MaterialInfo materialInfo;
	DecodeMaterialFromGBuffer(baseColor, 0.0, max(perceptualRoughness, 0.82), materialInfo);

	float3 color = float3(0, 0, 0);
	float4 mainLightClip = mul(float4(worldPos, 1.0), myPerFrame.Lights[0].LightViewProj);
	[loop]
	for (int i = 0; i < myPerFrame.LightCount; ++i)
	{
		Light light = myPerFrame.Lights[i];
		if (light.Type == LightType_Directional)
		{
			float4 lc = (i == 0) ? mainLightClip : mul(float4(worldPos, 1.0), light.LightViewProj);
			color += ApplyDirectionalLightHair(lc, light, baseColor, perceptualRoughness, aoDiffuse, strandT, geomN, view, coverageAlpha);
		}
		else if (light.Type == LightType_Point)
			color += ApplyPointLightHair(light, baseColor, perceptualRoughness, aoDiffuse, strandT, geomN, worldPos, view, i, coverageAlpha);
		else if (light.Type == LightType_Spot)
			color += ApplySpotLightHair(light, baseColor, perceptualRoughness, aoDiffuse, strandT, geomN, worldPos, view, coverageAlpha);
	}

	float3 iblDiffuse, iblSpecular;
	GetIBLContributionSplit(materialInfo, geomN, view, iblDiffuse, iblSpecular);
	float NdotVao = saturate(dot(geomN, view));
	float specOccPowBase = max(NdotVao + aoSpec - 0.0001, 1e-5);
	float specOcc = saturate(pow(specOccPowBase, exp2(-14.0 * perceptualRoughness - 0.62)) - 1.0 + aoSpec);
	float kkIbDiffuseMul = lerp(0.32, 0.72, perceptualRoughness);
	color += iblDiffuse * aoDiffuse * kkIbDiffuseMul * myPerFrame.IBLFactor;
	color += iblSpecular * specOcc * 0.038 * myPerFrame.IBLFactor;
	return color;
}

#endif // FUR_FORWARD_ACCUMULATE_HLSL
