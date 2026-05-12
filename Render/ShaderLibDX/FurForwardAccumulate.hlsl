// Fur forward accumulate: include after DeferredLightingShared (MaterialInfo, lights, IBL split).

#ifndef FUR_FORWARD_ACCUMULATE_HLSL
#define FUR_FORWARD_ACCUMULATE_HLSL

float3 AccumulateFurForwardShading(float3 worldPos, float3 geomN, float3 strandT, float3 view, float3 baseColor,
	float perceptualRoughness, float coverageAlpha, uint clusterIndex)
{
	const float aoDiffuse = 1.0;
	const float aoSpec = max(1.0, 0.2);
	MaterialInfo materialInfo;
	DecodeMaterialFromGBuffer(baseColor, 0.0, max(perceptualRoughness, 0.82), materialInfo);

	float3 color = float3(0, 0, 0);
	// Clustered Forward+: caller (FurMaterial MainPS) computes the cluster index from SV_Position before the shell
	// loop tessellates per-strand work; here we walk only the lights flagged by ClusterLightBuildCS for that cluster.
	const uint2 ClusterRange = _ClusterLightOffsetCount[clusterIndex];
	[loop]
	for (uint slot = 0u; slot < ClusterRange.y; ++slot)
	{
		const uint i = _ClusterLightIndexList[ClusterRange.x + slot];
		Light light = _SceneLights[i];
		if (light.Type == LightType_Directional)
			color += ApplyDirectionalLightHair(worldPos, light, baseColor, perceptualRoughness, aoDiffuse, strandT, geomN, view, coverageAlpha);
		else if (light.Type == LightType_Point)
			color += ApplyPointLightHair(light, baseColor, perceptualRoughness, aoDiffuse, strandT, geomN, worldPos, view, i, coverageAlpha);
		else if (light.Type == LightType_Spot)
			color += ApplySpotLightHair(light, baseColor, perceptualRoughness, aoDiffuse, strandT, geomN, worldPos, view, i, coverageAlpha);
	}

	// Forward shells: diffuse-only env (heavy roughness); match deferred split hemisphere when enabled.
	float3 DiffuseLight;
	if (myPerFrame.SplitHemisphereIBL != 0)
	{
		float pwr = max(myPerFrame.HemiIBLBlendPower, 0.08);
		float tN = saturate(geomN.y * 0.5 + 0.5);
		float wSkyN = pow(tN, pwr);
		float wGrN = pow(1.0 - tN, pwr);
		float sN = max(wSkyN + wGrN, 1e-4);
		wSkyN /= sN;
		wGrN /= sN;
		float3 irrSky = IrradianceTex.Sample(SampleLinear, geomN).rgb;
		float3 irrGr = GroundEnvLatLong.SampleLevel(SampleLinear, DirectionToLatLongUV(geomN), 0).rgb * myPerFrame.GroundIBLIntensity;
		DiffuseLight = irrSky * wSkyN + irrGr * wGrN;
	}
	else
		DiffuseLight = IrradianceTex.Sample(SampleLinear, geomN).rgb;
	float3 iblDiffuse = DiffuseLight * materialInfo.diffuseColor;
	float NdotVao = saturate(dot(geomN, view));
	float specOccPowBase = max(NdotVao + aoSpec - 0.0001, 1e-5);
	float specOcc = saturate(pow(specOccPowBase, exp2(-14.0 * perceptualRoughness - 0.62)) - 1.0 + aoSpec);
	float kkIbDiffuseMul = lerp(0.32, 0.72, perceptualRoughness);
	color += iblDiffuse * aoDiffuse * kkIbDiffuseMul * myPerFrame.IBLFactor;
	// Cheap stand-in for environment spec (one albedo mul, no extra texture fetches).
	float3 iblSpecularCheap = DiffuseLight * materialInfo.specularColor * 0.024;
	color += iblSpecularCheap * specOcc * myPerFrame.IBLFactor;
	return color;
}

#endif // FUR_FORWARD_ACCUMULATE_HLSL
