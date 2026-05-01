// Fullscreen deferred lighting: reads GBuffer from base pass, applies analytic lights + split-sum IBL.
#include "ShaderUtils.hlsl"
#include "PerFrameStruct.hlsl"

Texture2D BaseColorGBuffer : register(t0);
Texture2D NormalGBuffer : register(t1);
Texture2D EmissiveGBuffer : register(t2);
Texture2D MRGBuffer : register(t3);
Texture2D DepthTexture : register(t4);
TextureCube IrradianceTex : register(t5);
Texture2D BrdfLut : register(t6);
TextureCube PrefilterCubeMap : register(t7);
Texture2D ShadowMap : register(t8);
SamplerState SampleLinear : register(s0);
SamplerState SampleShadow : register(s1);

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
    if (Range < 0.0)
        return 1.0;
    return max(lerp(1.0, 0.0, Distance / Range), 0.0);
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

float Linstep(float a, float b, float v)
{
    return clamp((v - a) / (b - a), 0.0, 0.8);
}

float ReduceLightBleeding(float pMax, float amount)
{
    return Linstep(amount, 1.0f, pMax);
}

float ChebyshevUpperBound(float2 Moments, float t, float3 Normal)
{
    float Variance = Moments.y - Moments.x * Moments.x;
    float MinVariance = 0.0000001;
    Variance = max(Variance, MinVariance);
    float d = t - Moments.x;
    float pMax = Variance / (Variance + d * d);
    static const float lightBleedingReduction = 0.5;
    pMax = ReduceLightBleeding(pMax, lightBleedingReduction);
    pMax /= 0.8;
    float3 normal = normalize(Normal);
    float3 L = normalize(GetMainLight().Direction);
    float NdotL = abs(dot(normal, L));
    float baseBias = 0.005;
    float slopeBias = 0.01;
    float bias = baseBias + slopeBias * (1.0 - NdotL);
    bias = max(bias, 0.001);
    return (t - bias <= Moments.x ? 1.0 : pMax);
}

float ComputeShadow(float4 ShadowCoord, float3 Normal)
{
    float3 position = ShadowCoord.xyz / ShadowCoord.w;
    if (position.z <= 0.0 || position.z >= 1.0)
        return 1.0;
    position.xy = position.xy * float2(0.5, -0.5) + float2(0.5, 0.5);
    if (any(position.xy < 0.0) || any(position.xy > 1.0))
        return 1.0;
    float3 Moments = ShadowMap.Sample(SampleShadow, position.xy).xyz;
    float shadow = ChebyshevUpperBound(Moments.xy, clamp(position.z, 0.0, 1.0), Normal);
    return 1.0 - (1.0 - shadow) * Moments.z;
}

float3 ApplyDirectionalLightDeferred(float4 lightClipPos, Light light, MaterialInfo materialInfo, float3 normal, float3 view)
{
    float3 shade = GetPointShade(light.Direction, materialInfo, normal, view);
    float visibility = 1.0f;
    if (IsEnableShadow())
        visibility = clamp(ComputeShadow(lightClipPos, normal), 0.0, 1.0);
    return light.Intensity * light.Color * shade * visibility;
}

float3 ApplyPointLight(Light light, MaterialInfo materialInfo, float3 normal, float3 worldPos, float3 view)
{
    float3 pointToLight = light.Position - worldPos;
    float distance = length(pointToLight);
    float attenuation = GetRangeAttenuation(light.Range, distance);
    float3 shade = GetPointShade(pointToLight, materialInfo, normal, view);
    return attenuation * light.Intensity * light.Color * shade;
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

void DecodeMaterialFromGBuffer(float3 baseColor, float metallic, float perceptualRoughness, out MaterialInfo materialInfo)
{
    float3 f0 = float3(0.04, 0.04, 0.04);
    materialInfo.Metallic = metallic;
    materialInfo.perceptualRoughness = clamp(perceptualRoughness, 0.0, 1.0);
    materialInfo.alphaRoughness = materialInfo.perceptualRoughness * materialInfo.perceptualRoughness;
    materialInfo.diffuseColor = baseColor * (float3(1.0, 1.0, 1.0) - f0) * (1.0 - metallic);
    materialInfo.specularColor = lerp(f0, baseColor, metallic);
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
    float ao = mr.g;
    float perceptualRoughness = mr.b;

    float3 viewVec = myPerFrame.CameraPos.xyz - worldPos;
    float vLen = length(viewVec);
    float3 view = (vLen > 1e-5) ? (viewVec / vLen) : float3(0.0, 0.0, 1.0);
    MaterialInfo materialInfo;
    DecodeMaterialFromGBuffer(baseColor, metallic, perceptualRoughness, materialInfo);

    float3 color = float3(0, 0, 0);
    float4 mainLightClip = mul(float4(worldPos, 1.0), myPerFrame.Lights[0].LightViewProj);

    // LightCount is uniform but unknown at compile time; default unroll hits X3511 (~MAX_LIGHT_INSTANCES iter).
    [loop]
    for (int i = 0; i < myPerFrame.LightCount; ++i)
    {
        Light light = myPerFrame.Lights[i];
        if (light.Type == LightType_Directional)
        {
            float4 lc = (i == 0) ? mainLightClip : mul(float4(worldPos, 1.0), light.LightViewProj);
            color += ApplyDirectionalLightDeferred(lc, light, materialInfo, normal, view);
        }
        else if (light.Type == LightType_Point)
            color += ApplyPointLight(light, materialInfo, normal, worldPos, view);
        else if (light.Type == LightType_Spot)
            color += ApplySpotLight(light, materialInfo, normal, worldPos, view);
    }

    float3 iblDiffuse, iblSpecular;
    GetIBLContributionSplit(materialInfo, normal, view, iblDiffuse, iblSpecular);
    float NdotVao = saturate(dot(normal, view));
    // pow(negative, non-integer) -> NaN; breaks HDR and can TDR / stall device wait on exit.
    float specOccPowBase = max(NdotVao + ao - 0.0001, 1e-5);
    float specOcc = saturate(pow(specOccPowBase, exp2(-14.0 * perceptualRoughness - 0.62)) - 1.0 + ao);
    color += (iblDiffuse * ao + iblSpecular * specOcc) * myPerFrame.IBLFactor;
    color += emiss.rgb;

    return float4(color, alpha);
}
