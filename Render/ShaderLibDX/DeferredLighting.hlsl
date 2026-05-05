// Fullscreen deferred lighting: reads scene textures from base pass, applies analytic lights + split-sum IBL.
#include "ShaderUtils.hlsl"
#include "PerFrameStruct.hlsl"
#include "DeferredShadingCommon.hlsl"

Texture2D BaseColorGBuffer : register(t0);
Texture2D NormalGBuffer : register(t1);
Texture2D EmissiveGBuffer : register(t2);
Texture2D MRGBuffer : register(t3);
Texture2D DepthTexture : register(t4);
TextureCube IrradianceTex : register(t5);
Texture2D BrdfLut : register(t6);
TextureCube PrefilterCubeMap : register(t7);
Texture2D ShadowMap : register(t8);
Texture2D MaterialAuxGBuffer : register(t9);
TextureCube PointShadowCube : register(t10);
SamplerState SampleLinear : register(s0);
SamplerState SampleShadow : register(s1);

#include "ShadowPCSS.hlsl"

cbuffer cbPointShadow : register(b4)
{
    row_major matrix PointFaceVP[6];
    float4 PointShadowLightPosRange; // xyz = light pos, w = range (CPU-filled; matches Engine::CBPointShadow)
    int PointShadowEnabled;
    int PointShadowLightIndex;
    uint2 PointShadowPad;
};

#include "DeferredLightingShared.hlsl"

struct PSInput
{
    float2 Tex : TEXCOORD;
    float4 Pos : SV_Position;
};

// Fullscreen triangle; must live in this file so VS/PS share cbPerFrame at b0 (not PostProcess BloomContants).
PSInput VS_ScreenQuad(uint VertID : SV_VertexID)
{
    PSInput Out;
    float2 Tex = float2(uint2(VertID, VertID << 1) & 2);
    Out.Tex = Tex;
    Out.Pos = float4(lerp(float2(-1, 1), float2(1, -1), Tex), 0, 1);
    return Out;
}

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

float3 ApplyDirectionalLightDeferred(float4 lightClipPos, Light light, MaterialInfo materialInfo, float3 normal, float3 view)
{
    float3 shade = GetPointShade(light.Direction, materialInfo, normal, view);
    float visibility = 1.0f;
    if (IsEnableShadow())
        visibility = clamp(ComputeShadow(lightClipPos, normal), 0.0, 1.0);
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

float3 ApplySpotLight(Light light, MaterialInfo materialInfo, float3 normal, float3 worldPos, float3 view)
{
	float3 pointToLight = light.Position - worldPos;
	float distance = length(pointToLight);
	float rangeAttenuation = GetRangeAttenuation(light.Range, distance);
	float spotAttenuation = GetSpotAttenuation(pointToLight, -light.Direction, light.OuterConeCos, light.InnerConeCos);
	float3 shade = GetPointShade(pointToLight, materialInfo, normal, view);
	return rangeAttenuation * spotAttenuation * light.Intensity * light.Color * shade;
}

float3 ReconstructWorldPosition(float2 uv, float depthHw)
{
    // Hardware depth in [0,1] with clear=1 (far). Row-vector clip: world * VP = clip, clip.w = view-space Z
    // for standard LH perspective. Recover clip before divide, then inv(VP) -> world (matches VS mul(world, VP)).
    float2 ndcXY = float2(uv.x * 2.0 - 1.0, 1.0 - uv.y * 2.0);
    float n = myPerFrame.CameraNearZ;
    float f = myPerFrame.CameraFarZ;
    float denom = max(f - depthHw * (f - n), 1e-6);
    float clipW = (n * f) / denom;
    float4 clipH = float4(ndcXY.x * clipW, ndcXY.y * clipW, depthHw * clipW, clipW);
    float4 w = mul(clipH, myPerFrame.CameraCurrViewProjInverse);
    return w.xyz / max(w.w, 1e-5);
}

float4 PS_DeferredLighting(PSInput Input) : SV_Target0
{
	float2 uv = Input.Tex;
	float depth = DepthTexture.Sample(SampleLinear, uv).r;
	float4 baseSample = BaseColorGBuffer.Sample(SampleLinear, uv);
	float3 baseColor = baseSample.rgb;
	float alpha = baseSample.a;

	if (depth >= 0.99999)
		return float4(baseColor, alpha);

	float3 worldPos = ReconstructWorldPosition(uv, depth);
	float3 packedN = NormalGBuffer.Sample(SampleLinear, uv).xyz;
	float3 nUnnorm = packedN * 2.0 - 1.0;
	float nLen = length(nUnnorm);
	float3 normal = (nLen > 1e-5) ? (nUnnorm / nLen) : float3(0.0, 0.0, 1.0);
	float4 emiss = EmissiveGBuffer.Sample(SampleLinear, uv);
	float4 mr = MRGBuffer.Sample(SampleLinear, uv);
	float metallic = mr.r;
	float aoRaw = mr.g;
	float perceptualRoughness = mr.b;
	// Aggressive baked AO (often 0 in crevices) drives specOcc to ~0 and removes all IBL specular. Floor for spec path only.
	const float aoDiffuse = max(aoRaw, 1e-4);
	const float aoSpec = max(aoRaw, 0.2);

	float3 viewVec = myPerFrame.CameraPos.xyz - worldPos;
	float vLen = length(viewVec);
	float3 view = (vLen > 1e-5) ? (viewVec / vLen) : float3(0.0, 0.0, 1.0);
	MaterialInfo materialInfo;
	DecodeMaterialFromGBuffer(baseColor, metallic, perceptualRoughness, materialInfo);

	float4 auxSample = MaterialAuxGBuffer.Sample(SampleLinear, uv);
	uint smid = DecodeShadingModelId(auxSample.r);
	const bool bHair = IsHairShadingModel(smid);
	float3 strandT = DecodeHairTangentOctPacked(auxSample.gb);
	// MatAux .a is blended with shell coverage (SrcAlpha MRT); do not use as packed ibl scale for hair.
	float hairIblDiffuseScale = bHair ? 1.0f : saturate(auxSample.a);

	// Fur/shell edges: SrcAlpha-blended normals between strands and contact geometry crush N·L on default-lit GGX. Nudge toward
	// world up for dielectric, high-roughness semi-transparent pixels (not hair SM — aux channel must stay interpretable).
	if (!bHair && baseSample.a < 0.999f && baseSample.a > 1e-3f && metallic < 0.05f && perceptualRoughness > 0.5f)
	{
		float edge = saturate((1.0f - baseSample.a) * 3.0f);
		float3 upW = float3(0.0f, 1.0f, 0.0f);
		normal = normalize(lerp(normal, upW, edge * 0.45f));
	}
	// Hair/fur: same blended-geomN issue on floor; sky often skips lighting (depth early-out). Nudge for IBL + shadow axes.
	if (bHair && baseSample.a < 0.999f && baseSample.a > 1e-3f)
	{
		float edge = saturate((1.0f - baseSample.a) * 3.0f);
		float3 upW = float3(0.0f, 1.0f, 0.0f);
		normal = normalize(lerp(normal, upW, edge * 0.42f));
	}

	float3 color = float3(0, 0, 0);
	float4 mainLightClip = mul(float4(worldPos, 1.0), myPerFrame.Lights[0].LightViewProj);

	[loop]
	for (int i = 0; i < myPerFrame.LightCount; ++i)
	{
		Light light = myPerFrame.Lights[i];
		if (bHair)
		{
			if (light.Type == LightType_Directional)
			{
				float4 lc = (i == 0) ? mainLightClip : mul(float4(worldPos, 1.0), light.LightViewProj);
				color += ApplyDirectionalLightHair(lc, light, baseColor, perceptualRoughness, aoDiffuse, strandT, normal, view, baseSample.a);
			}
			else if (light.Type == LightType_Point)
				color += ApplyPointLightHair(light, baseColor, perceptualRoughness, aoDiffuse, strandT, normal, worldPos, view, i, baseSample.a);
			else if (light.Type == LightType_Spot)
				color += ApplySpotLightHair(light, baseColor, perceptualRoughness, aoDiffuse, strandT, normal, worldPos, view, baseSample.a);
		}
		else
		{
			if (light.Type == LightType_Directional)
			{
				float4 lc = (i == 0) ? mainLightClip : mul(float4(worldPos, 1.0), light.LightViewProj);
				color += ApplyDirectionalLightDeferred(lc, light, materialInfo, normal, view);
			}
			else if (light.Type == LightType_Point)
				color += ApplyPointLight(light, materialInfo, normal, worldPos, view, i);
			else if (light.Type == LightType_Spot)
				color += ApplySpotLight(light, materialInfo, normal, worldPos, view);
		}
	}

	float3 iblDiffuse, iblSpecular;
	GetIBLContributionSplit(materialInfo, normal, view, iblDiffuse, iblSpecular);
	float NdotVao = saturate(dot(normal, view));
	float specOccPowBase = max(NdotVao + aoSpec - 0.0001, 1e-5);
	float specOcc = saturate(pow(specOccPowBase, exp2(-14.0 * perceptualRoughness - 0.62)) - 1.0 + aoSpec);

	if (bHair)
	{
		float kkIbDiffuseMul = lerp(0.32, 0.72, perceptualRoughness);
		color += iblDiffuse * aoDiffuse * hairIblDiffuseScale * kkIbDiffuseMul * myPerFrame.IBLFactor;
		color += iblSpecular * specOcc * 0.14 * myPerFrame.IBLFactor;
	}
	else
		color += (iblDiffuse * aoDiffuse + iblSpecular * specOcc) * myPerFrame.IBLFactor;

	color += emiss.rgb;

	return float4(color, alpha);
}
