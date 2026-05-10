// Forward-shaded translucent PBR after deferred lighting (UE4-style separate translucency).
// Bindings: material t0–t4 + cbPerMaterial b6 (PBRMaterialSampling); IBL + shadows t5–t8, t10–t11 + b4/b5/b7 (matches DeferredLightingPass::BindFurForwardSharedSRVs).

#include "ShaderUtils.hlsl"
#include "GLTFPbrPass-VS.hlsl"
#include "PBRMaterialSampling.hlsl"

TextureCube IrradianceTex : register(t5);
Texture2D BrdfLut : register(t6);
TextureCube PrefilterCubeMap : register(t7);
Texture2D ShadowMap : register(t8);
TextureCube PointShadowCube : register(t10);
Texture2D GroundEnvLatLong : register(t12);

SamplerState SampleShadow : register(s1);

#include "ShadowPCSS.hlsl"

cbuffer cbDirectionalShadowCSM : register(b7)
{
	row_major matrix CascadeViewProj[3];
	float4 CascadeSplits;
	float4 CameraForwardInvCount;
	int DirectionalCSMEnabled;
	int3 _PadDirectionalCSM;
};

#include "DirectionalShadowCSM.hlsl"

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

float4 MainPS_TranslucentForward(VS_OUTPUT_SCENE Input) : SV_Target0
{
	float3 worldPos = Input.WorldPos;
	float3 normal = getPixelNormal(Input);

	float alpha;
	float perceptualRoughness;
	float metallic;
	float3 diffuseColor;
	float3 specularColor;
	GetPBRParams(Input, diffuseColor, specularColor, perceptualRoughness, metallic, alpha);

	if (myMaterial.AlphaMask != 0)
		clip(alpha - myMaterial.AlphaCutoff);

	if (myPerFrame.bUnlit != 0)
	{
		float4 bc = AlbedoMap.Sample(SampleLinear, Input.UV0);
		float3 albedoLin = sRGBToLinear(bc.rgb);
		float3 emLin = sRGBToLinear(EmissMap.Sample(SampleLinear, Input.UV0).rgb);
		return float4(albedoLin + emLin, bc.a);
	}

	float4 baseTex = AlbedoMap.Sample(SampleLinear, Input.UV0);
	float3 linearBase = sRGBToLinear(baseTex.rgb);
	MaterialInfo materialInfo;
	DecodeMaterialFromGBuffer(linearBase, metallic, perceptualRoughness, materialInfo);

	float4 aoSamp = AoMap.Sample(SampleLinear, Input.UV0);
	float aoRaw = max(max(aoSamp.r, aoSamp.g), aoSamp.b);
	const float aoDiffuse = max(aoRaw, 1e-4);
	const float aoSpec = max(aoRaw, 0.2);

	float3 viewVec = myPerFrame.CameraPos.xyz - worldPos;
	float vLen = length(viewVec);
	float3 view = (vLen > 1e-5) ? (viewVec / vLen) : float3(0.0, 0.0, 1.0);

	float3 color = float3(0, 0, 0);

	[loop]
	for (int i = 0; i < myPerFrame.LightCount; ++i)
	{
		Light light = myPerFrame.Lights[i];
		if (light.Type == LightType_Directional)
			color += ApplyDirectionalLightDeferred(worldPos, light, materialInfo, normal, view);
		else if (light.Type == LightType_Point)
			color += ApplyPointLight(light, materialInfo, normal, worldPos, view, i);
		else if (light.Type == LightType_Spot)
			color += ApplySpotLight(light, materialInfo, normal, worldPos, view, i);
	}

	float3 iblDiffuse, iblSpecular;
	GetIBLContributionSplit(materialInfo, normal, view, iblDiffuse, iblSpecular);
	float NdotVao = saturate(dot(normal, view));
	float specOccPowBase = max(NdotVao + aoSpec - 0.0001, 1e-5);
	float specOcc = saturate(pow(specOccPowBase, exp2(-14.0 * perceptualRoughness - 0.62)) - 1.0 + aoSpec);

	color += (iblDiffuse * aoDiffuse + iblSpecular * specOcc) * myPerFrame.IBLFactor;

	float3 emiss = EmissMap.Sample(SampleLinear, Input.UV0).rgb;
	color += emiss.rgb;

	return float4(color, alpha);
}
