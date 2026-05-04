// Fullscreen deferred lighting: reads scene textures from base pass, applies analytic lights + split-sum IBL.
#include "ShaderUtils.hlsl"
#include "PerFrameStruct.hlsl"
#include "DeferredShadingCommon.hlsl"
#include "HairShading.hlsl"

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

static const int kPointLightCubeShadowMapIndex = 2;

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

float GetRangeAttenuation(float Range, float Distance)
{
    // Single accumulation variable keeps FXC happy (avoids X4000 on split returns).
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
    // Avoid identifier 'shadow' (reserved context); single writer avoids X4000 flow bugs.
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

float DirectionalShadowHair(float4 lightClipPos, float3 geomN)
{
	float visibility = 1.0f;
	if (IsEnableShadow())
		visibility = clamp(ComputeShadow(lightClipPos, geomN), 0.0, 1.0);
	return visibility;
}

float3 ApplyDirectionalLightHair(float4 lightClipPos, Light light, float3 baseColor, float perceptualRoughness, float ao,
	float3 strandT, float3 geomN, float3 view)
{
	float3 L = normalize(light.Direction);
	float NdotL = saturate(dot(geomN, L));
	float3 diffKK, specKK;
	KajiyaKayTerms(strandT, L, view, perceptualRoughness, baseColor, diffKK, specKK);
	float visibility = DirectionalShadowHair(lightClipPos, geomN);
	return light.Intensity * light.Color * (diffKK + specKK) * NdotL * ao * visibility;
}

float3 ApplyPointLightHair(Light light, float3 baseColor, float perceptualRoughness, float ao,
	float3 strandT, float3 geomN, float3 worldPos, float3 view, int lightIndex)
{
	float3 pointToLight = light.Position - worldPos;
	float distance = length(pointToLight);
	float3 L = pointToLight / max(distance, 1e-5);
	float attenuation = GetRangeAttenuation(light.Range, distance);
	float NdotL = saturate(dot(geomN, L));
	float3 diffKK, specKK;
	KajiyaKayTerms(strandT, L, view, perceptualRoughness, baseColor, diffKK, specKK);
	float vis = 1.0;
	if (light.ShadowMapIndex == kPointLightCubeShadowMapIndex && PointShadowEnabled != 0 && lightIndex == PointShadowLightIndex)
		vis = SamplePointShadowCubeVisibility(worldPos, light.Position, light.Range);
	return attenuation * light.Intensity * light.Color * (diffKK + specKK) * NdotL * ao * vis;
}

float3 ApplySpotLightHair(Light light, float3 baseColor, float perceptualRoughness, float ao,
	float3 strandT, float3 geomN, float3 worldPos, float3 view)
{
	float3 pointToLight = light.Position - worldPos;
	float distance = length(pointToLight);
	float rangeAttenuation = GetRangeAttenuation(light.Range, distance);
	float spotAttenuation = GetSpotAttenuation(pointToLight, -light.Direction, light.OuterConeCos, light.InnerConeCos);
	float3 L = pointToLight / max(distance, 1e-5);
	float NdotL = saturate(dot(geomN, L));
	float3 diffKK, specKK;
	KajiyaKayTerms(strandT, L, view, perceptualRoughness, baseColor, diffKK, specKK);
	return rangeAttenuation * spotAttenuation * light.Intensity * light.Color * (diffKK + specKK) * NdotL * ao;
}

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
	float hairIblDiffuseScale = saturate(auxSample.a);

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
				color += ApplyDirectionalLightHair(lc, light, baseColor, perceptualRoughness, aoDiffuse, strandT, normal, view);
			}
			else if (light.Type == LightType_Point)
				color += ApplyPointLightHair(light, baseColor, perceptualRoughness, aoDiffuse, strandT, normal, worldPos, view, i);
			else if (light.Type == LightType_Spot)
				color += ApplySpotLightHair(light, baseColor, perceptualRoughness, aoDiffuse, strandT, normal, worldPos, view);
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
