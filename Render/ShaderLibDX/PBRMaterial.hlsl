#include "EnvironmentShaders.hlsl"
#include "GLTFPbrPass-VS.hlsl"
#include "GLTFPbrPass-IO.hlsl"

Texture2D AlbedoMap : register(t0);
Texture2D NormalMap : register(t1);
Texture2D Roughness_metallicMap : register(t2);
Texture2D EmissMap : register(t3);
Texture2D AoMap : register(t4);
TextureCube IrradianceTex : register(t5);
Texture2D BrdfLut : register(t6);
TextureCube PrefliterCubeMap : register(t7);
Texture2D ShadowMap: register(t8);
SamplerState SampleLinear : register(s0);
SamplerState SampleShadow : register(s1);

struct PS_OUTPUT_SCENE
{
    float4 Target0 : SV_Target0; //Scene color
    float4 Target1 : SV_Target1; //Velocity buffer
    float4 Target2 : SV_Target2; //Normal
    float4 Target3 : SV_Target3; //Emissive
    float4 Target4 : SV_Target4; //metallSpecularRoughness
};

struct MaterialInfo
{
    float perceptualRoughness; // roughness value, as authored by the model creator (input to shader)
    float3 reflectance0; // full reflectance color (normal incidence angle)

    float alphaRoughness; // roughness mapped to a more linear change in the roughness (proposed by [2])
    float3 diffuseColor; // color contribution from diffuse lighting

    float3 reflectance90; // reflectance color at grazing angle
    float3 specularColor; // color contribution from specular lighting
    float Metallic;
};


// Calculation of the lighting contribution from an optional Image Based Light source.

float3 GetIBLContribution(MaterialInfo MaterialInfo, float3 n, float3 v)
{
    float NdotV = clamp(dot(n, v), 0.0, 1.0);

    float u_MipCount = myPerFrame.IBLMIpCount; // resolution of 512x512 of the IBL
    float lod = clamp(MaterialInfo.perceptualRoughness * float(u_MipCount), 0.0, float(u_MipCount));
    float3 reflection = normalize(reflect(-v, n));
    reflection = mul(float4(reflection, 1.0), myPerFrame.RotateIBL).xyz;
    float Mip = ComputeReflectionCaptureMipFromRoughness(MaterialInfo.perceptualRoughness, u_MipCount - 1);
    
    float2 brdfSamplePoint = clamp(float2(NdotV, Mip), float2(0.0, 0.0), float2(1.0, 1.0));

    float2 BRDF = BrdfLut.Sample(SampleLinear, brdfSamplePoint).rg;

    float3 DiffuseLight = IrradianceTex.Sample(SampleLinear, n).rgb;
    // 压掉 HDR 漫反射环境在局部法线上的尖峰，减轻「一片白」的 IBL 观感，同时保留暗部能量
    float diffLum = dot(DiffuseLight, float3(0.2126, 0.7152, 0.0722));
    DiffuseLight *= rcp(1.0 + diffLum * 0.38);

    float3 SpecularLight = PrefliterCubeMap.SampleLevel(SampleLinear, reflection, lod).rgb;

    float3 Diffuse = DiffuseLight * MaterialInfo.diffuseColor * (1.0 - MaterialInfo.Metallic);
    float3 Specular = SpecularLight * (MaterialInfo.specularColor * BRDF.x + BRDF.y) * MaterialInfo.Metallic;

    float iblScale = myPerFrame.Material.IBLMaterialScale * myPerFrame.IBLFactor;
    return (Diffuse + Specular) * iblScale;
}

// Lambert lighting
// see https://seblagarde.wordpress.com/2012/01/08/pi-or-not-to-pi-in-game-lighting-equation/
float3 Diffuse(MaterialInfo materialInfo)
{
    return materialInfo.diffuseColor / PI;
}

// The following equation models the Fresnel reflectance term of the spec equation (aka F())
// Implementation of fresnel from [4], Equation 15
float3 SpecularReflection(MaterialInfo MaterialInfo, AngularInfo angularInfo)
{
    return MaterialInfo.reflectance0 + (MaterialInfo.reflectance90 - MaterialInfo.reflectance0) * pow(clamp(1.0 - angularInfo.VdotH, 0.0, 1.0), 5.0);
}

// Smith Joint GGX
// Note: Vis = G / (4 * NdotL * NdotV)
// see Eric Heitz. 2014. Understanding the Masking-Shadowing Function in Microfacet-Based BRDFs. Journal of Computer Graphics Techniques, 3
// see Real-Time Rendering. Page 331 to 336.
// see https://google.github.io/filament/Filament.md.html#materialsystem/specularbrdf/geometricshadowing(specularg)
float VisibilityOcclusion(MaterialInfo MaterialInfo, AngularInfo AngularInfo)
{
    float NdotL = AngularInfo.NdotL;
    float NdotV = AngularInfo.NdotV;
    float alphaRoughnessSq = MaterialInfo.alphaRoughness * MaterialInfo.alphaRoughness;

    float GGXV = NdotL * sqrt(NdotV * NdotV * (1.0 - alphaRoughnessSq) + alphaRoughnessSq);
    float GGXL = NdotV * sqrt(NdotL * NdotL * (1.0 - alphaRoughnessSq) + alphaRoughnessSq);

    float GGX = GGXV + GGXL;
    if (GGX > 0.0)
    {
        return 0.5 / GGX;
    }
    return 0.0;
}

// The following equation(s) model the distribution of microfacet normals across the area being drawn (aka D())
// Implementation from "Average Irregularity Representation of a Roughened Surface for Ray Reflection" by T. S. Trowbridge, and K. P. Reitz
// Follows the distribution function recommended in the SIGGRAPH 2013 course notes from EPIC Games [1], Equation 3.
float MicrofacetDistribution(MaterialInfo MaterialInfo, AngularInfo AngularInfo)
{
    float alphaRoughnessSq = MaterialInfo.alphaRoughness * MaterialInfo.alphaRoughness;
    float f = (AngularInfo.NdotH * alphaRoughnessSq - AngularInfo.NdotH) * AngularInfo.NdotH + 1.0;
    return alphaRoughnessSq / (PI * f * f + 0.000001f);
}

float3 GetPointShade(float3 PointToLight, MaterialInfo MaterialInfo, float3 Normal, float3 View)
{
    AngularInfo angularInfo = GetAngularInfo(PointToLight, Normal, View);

    if (angularInfo.NdotL > 0.0 || angularInfo.NdotV > 0.0)
    {
        // Calculate the shading terms for the microfacet specular shading model
        float3 F = SpecularReflection(MaterialInfo, angularInfo);
        float Vis = VisibilityOcclusion(MaterialInfo, angularInfo);
        float D = MicrofacetDistribution(MaterialInfo, angularInfo);
        
        // Calculation of analytical lighting contribution
        float3 diffuseContrib = (1.0 - F) * Diffuse(MaterialInfo);
        float3 specContrib = F * Vis * D;
        // 高粗糙度电介质上法线贴图易把解析高光拉成窄条，略衰减以减轻「镜面感」
        float roughAtten = saturate((MaterialInfo.perceptualRoughness - 0.5) / 0.48);
        specContrib *= lerp(1.0, 0.42, roughAtten * (1.0 - MaterialInfo.Metallic));

        // Obtain final intensity as reflectance (BRDF) scaled by the energy of the light (cosine law)
        return angularInfo.NdotL * (diffuseContrib + specContrib);
    }

    return float3(0.0, 0.0, 0.0);
}

// https://github.com/KhronosGroup/glTF/blob/master/extensions/2.0/Khronos/KHR_lights_punctual/README.md#range-property
float GetRangeAttenuation(float Range, float Distance)
{
    if (Range < 0.0)
    {
        // negative range means unlimited
        return 1.0;
    }
    return max(lerp(1, 0, Distance / Range), 0);
    //return max(min(1.0 - pow(distance / range, 4.0), 1.0), 0.0) / pow(distance, 2.0);
}

// https://github.com/KhronosGroup/glTF/blob/master/extensions/2.0/Khronos/KHR_lights_punctual/README.md#inner-and-outer-cone-angles
float GetSpotAttenuation(float3 PointToLight, float3 SpotDirection, float OuterConeCos, float InnerConeCos)
{
    float actualCos = dot(normalize(SpotDirection), normalize(-PointToLight));
    if (actualCos > OuterConeCos)
    {
        if (actualCos < InnerConeCos)
        {
            return smoothstep(OuterConeCos, InnerConeCos, actualCos);
        }
        return 1.0;
    }
    return 0.0;
}

float Linstep(float a, float b, float v)
{
	return clamp((v - a) / (b - a), 0.0, 0.8);
}
// Reduces VSM light bleedning
float ReduceLightBleeding(float pMax, float amount)
{
	// Remove the [0, amount] tail and linearly rescale (amount, 1].
	return Linstep(amount, 1.0f, pMax);
}

float ChebyshevUpperBound(float2 Moments, float t, float3 Normal)
{
	float Variance = Moments.y - Moments.x * Moments.x;
	float MinVariance = 0.0000001;
	Variance = max(Variance, MinVariance);

	// Compute probabilistic upper bound.
	float d = t - Moments.x;
	float pMax = Variance / (Variance + d * d);

	// 可配置的light bleeding reduction参数（可以在uniform buffer中传递）
	static const float lightBleedingReduction = 0.5;
	pMax = ReduceLightBleeding(pMax, lightBleedingReduction);

	pMax /= 0.8;
	
	// 改进的Slope-Scale Depth Bias：考虑表面法线与光照方向的夹角
	float3 normal = normalize(Normal);
	float3 L = normalize(GetMainLight().Direction);
	float NdotL = abs(dot(normal, L));
	
	// 当表面近乎平行于光照方向时，使用更大的bias
	// Slope-scale bias: bias = baseBias + slopeBias * tan(theta)
	// theta 是表面法线与光照方向的夹角
	float baseBias = 0.005;
	float slopeBias = 0.01;
	float bias = baseBias + slopeBias * (1.0 - NdotL);
	
	// 使用接收平面深度bias（Receiver Plane Depth Bias）以进一步减少shadow acne
	// 这里简化处理，实际可以通过计算深度梯度来获得更精确的bias
	bias = max(bias, 0.001);
	
	return (t - bias <= Moments.x ? 1.0 : pMax);
}

float ComputeShadow(float4 ShadowCoord, float3 Normal)
{
	float3 position = ShadowCoord.xyz / ShadowCoord.w;
	// Outside shadow clip: treat as lit (avoid broken banding at frustum edges).
	if (position.z <= 0.0 || position.z >= 1.0)
		return 1.0;
	position.xy = position.xy * float2(0.5, -0.5) + float2(0.5, 0.5);
	if (any(position.xy < 0.0) || any(position.xy > 1.0))
		return 1.0;

	float3 Moments = ShadowMap.Sample(SampleShadow, position.xy).xyz;
	float shadow =  ChebyshevUpperBound(Moments.xy, clamp(position.z, 0.0, 1.0), Normal);
    return 1.0 - (1.0 - shadow) * Moments.z;
	// return shadow * Moments.z;
}

float3 ApplyDirectionalLight(VS_OUTPUT_SCENE Input,Light light, MaterialInfo materialInfo, float3 normal, float3 view)
{
    float3 pointToLight = light.Direction;
    float3 shade = GetPointShade(pointToLight, materialInfo, normal, view);
    float visibility = 1.0f;
    if (IsEnableShadow())
		visibility = clamp(ComputeShadow(Input.LightPos,normal),0.0,1.0);
    return light.Intensity * light.Color * shade*visibility;
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

float ComputeDielectricF0(float reflectance)
{
    return 0.16 * reflectance * reflectance;
}

float3 ComputeF0(const float4 baseColor, float metallic, float reflectance)
{
    return baseColor.rgb * metallic + (reflectance * (1.0 - metallic));
}

float3 getNormalTexture(VS_OUTPUT_SCENE Input)
{
    float2 xy = 2.0 * NormalMap.SampleBias(SampleLinear, Input.UV0, myPerFrame.LodBias).rg - 1.0;
    float len2 = dot(xy, xy);
    len2 = min(len2, 0.999999);
    float z = sqrt(1.0 - len2);
    return float3(xy, z);
}

// Find the normal for this fragment, pulling either from a predefined normal map
// or from the interpolated mesh normal and tangent attributes.
float3 getPixelNormal(VS_OUTPUT_SCENE Input, bool bIsFontFacing = false)
{
    // Retrieve the tangent space matrix
#ifndef HAS_TANGENT
    float2 UV = Input.UV0;
    float3 pos_dx = ddx(Input.WorldPos);
    float3 pos_dy = ddy(Input.WorldPos);
    float3 tex_dx = ddx(float3(UV, 0.0));
    float3 tex_dy = ddy(float3(UV, 0.0));
    float denom = (tex_dx.x * tex_dy.y - tex_dy.x * tex_dx.y);
    float3 ng = normalize(Input.Normal);
    float3 t, b;
    if (abs(denom) < 1e-5)
    {
        float3 ref = abs(ng.y) < 0.999f ? float3(0, 1, 0) : float3(1, 0, 0);
        t = normalize(cross(ref, ng));
        b = normalize(cross(ng, t));
    }
    else
    {
        t = (tex_dy.y * pos_dx - tex_dx.y * pos_dy) / denom;
        t = normalize(t - ng * dot(ng, t));
        b = normalize(cross(ng, t));
    }
    float3x3 tbn = float3x3(t, b, ng);
#else // HAS_TANGENT
    float3 N = normalize(Input.Normal);
    float3 Tw = Input.Tangent;
    float3 T = normalize(Tw - N * dot(N, Tw));
    float3 B = cross(N, T);
    if (dot(B, Input.Binormal) < 0.0)
        B = -B;
    float3x3 tbn = float3x3(T, B, N);
#endif

    float3 n = getNormalTexture(Input);
    n = normalize(mul(tbn, n));

    return n * (bIsFontFacing ? -1 : 1);
}

float3 DoPbrLighting(VS_OUTPUT_SCENE Input, in PerFrame perFrame, in float3 diffuseColor, in float3 specularColor, in float perceptualRoughness, in float metallic)
{
#ifdef MATERIAL_UNLIT
        return AlbedoMap.Sample(SampleLinear, Input.UV0).rgb;
#endif

    // Roughness is authored as perceptual roughness; as is convention,
    // convert to material roughness by squaring the perceptual roughness [2].
    float alphaRoughness = perceptualRoughness * perceptualRoughness;
    
    float3 specularEnvironmentR0 = specularColor.rgb;
    // Anything less than 2% is physically impossible and is instead considered to be shadowing. Compare to "Real-Time-Rendering" 4th editon on page 325.
    float reflectance = max(max(specularColor.r, specularColor.g), specularColor.b);
    float3 specularEnvironmentR90 = float3(1.0, 1.0, 1.0) * clamp(reflectance * 50.0, 0.0, 1.0);

    MaterialInfo materialInfo =
    {
        perceptualRoughness,
        specularEnvironmentR0,
        alphaRoughness,
        diffuseColor,
        specularEnvironmentR90,
        specularColor,
        metallic,
    };

    // LIGHTING（AO 只乘 IBL，与 glTF 常见用法一致，避免主光被 AO 压成死黑）
    float3 directLighting = float3(0.0, 0.0, 0.0);
    float3 normal = getPixelNormal(Input);
    float3 worldPos = Input.WorldPos;
    float3 view = normalize(perFrame.CameraPos.xyz - worldPos);

#if (DEF_doubleSided == 1)
    if (dot(normal, view) < 0)
    {
        normal = -normal;
    }
#endif

    for (int i = 0; i < perFrame.LightCount; ++i)
    {
        Light light = perFrame.Lights[i];
        float shadowFactor = 1.0f;
       // float shadowFactor = CalcShadows(Input.WorldPos.xyz, int2(Input.svPosition.xy), light);
        if (light.Type == LightType_Directional)
        {
            directLighting += ApplyDirectionalLight(Input,light, materialInfo, normal, view) * shadowFactor;
        }
        else if (light.Type == LightType_Point)
        {
            directLighting += ApplyPointLight(light, materialInfo, normal, worldPos, view) * shadowFactor;
        }
        else if (light.Type == LightType_Spot)
        {
            directLighting += ApplySpotLight(light, materialInfo, normal, worldPos, view) * shadowFactor;
        }
    }

    float3 iblLighting = GetIBLContribution(materialInfo, normal, view);

    float ao = 1.0;
    // Apply optional PBR terms for additional (optional) shading
    ao = AoMap.Sample(SampleLinear, Input.UV0).r;
    float3 color = directLighting + iblLighting * ao;


#ifndef DEBUG_OUTPUT // no debug
    // regular shading
    float3 outColor = color;

#else // debug output

#ifdef DEBUG_METALLIC
    outColor.rgb = float3(metallic);
#endif

#ifdef DEBUG_ROUGHNESS
    outColor.rgb = float3(perceptualRoughness);
#endif

#ifdef DEBUG_NORMAL
#ifdef ID_normalTexture
    outColor.rgb = texture(u_NormalSampler, getNormalUV(Input)).rgb;
#else
    outColor.rgb = float3(0.5, 0.5, 1.0);
#endif
#endif

#ifdef DEBUG_BASECOLOR
    outColor.rgb = (diffuseColor.rgb);
#endif

#ifdef DEBUG_OCCLUSION
    outColor.rgb = float3(ao);
#endif

#ifdef DEBUG_EMISSIVE
    outColor.rgb = (emissive);
#endif

#ifdef DEBUG_F0
    outColor.rgb = float3(f0);
#endif

#ifdef DEBUG_ALPHA
    outColor.rgb = float3(alpha);
#endif

#endif // !DEBUG_OUTPUT

    return outColor;
}


void GetPBRParams(VS_OUTPUT_SCENE Input,out float3 diffuseColor, out float3 specularColor, out float perceptualRoughness,out float metallic, out float alpha)
{
    // Metallic and Roughness material properties are packed together
    // In glTF, these factors can be specified by fixed scalar values
    // or from a metallic-roughness map
    alpha = 0.0;
    perceptualRoughness = 0.0;
    diffuseColor = float3(0.0, 0.0, 0.0);
    specularColor = float3(0.0, 0.0, 0.0);
    float3 f0 = float3(0.04, 0.04, 0.04);

    float4 baseColor = AlbedoMap.Sample(SampleLinear, Input.UV0);
    
    float4 mr = Roughness_metallicMap.Sample(SampleLinear, Input.UV0);
    perceptualRoughness = mr.g;
    metallic = mr.b;
    if (myPerFrame.Material.MROverride != 0)
    {
        perceptualRoughness = myPerFrame.Material.Roughness;
        metallic = myPerFrame.Material.Metallic;
    }

    // Roughness is stored in the 'g' channel, metallic is stored in the 'b' channel.
    // This layout intentionally reserves the 'r' channel for (optional) occlusion map data

    diffuseColor = baseColor.rgb * (float3(1.0, 1.0, 1.0) - f0) * (1.0 - metallic);
    specularColor = lerp(f0, baseColor.rgb, metallic);

    perceptualRoughness = clamp(perceptualRoughness, 0.0, 1.0);

    alpha = baseColor.a;
}


float3 Calculate3DVelocity(float4 CurrentVelocity, float4 PreVelocity)
{
	// minus jitter
    float2 ScreenPos = CurrentVelocity.xy / CurrentVelocity.w - myPerFrame.TemporalAAJitter.xy;
    float2 PrevScreenPos = PreVelocity.xy / PreVelocity.w - myPerFrame.TemporalAAJitter.zw;

    float DeviceZ = CurrentVelocity.z / CurrentVelocity.w;
    float PrevDeviceZ = PreVelocity.z / PreVelocity.w;

	// 3d velocity, includes camera an object motion
    float3 Velocity = float3(ScreenPos - PrevScreenPos, DeviceZ - PrevDeviceZ);
	//Velocity.xy = float2(0.5f, -0.5f) * Velocity.xy;
	//Velocity.xy *= float2(1024, 768);

	// Make sure not to touch 0,0 which is clear color
    return Velocity;
}


PS_OUTPUT_SCENE MainPS(VS_OUTPUT_SCENE Input) : SV_Target
{
	PS_OUTPUT_SCENE Output;
    
    float alpha;
    float perceptualRoughness;
    float3 diffuseColor;
    float3 specularColor;
    float metallic;
    GetPBRParams(Input, diffuseColor, specularColor, perceptualRoughness,metallic, alpha);

    float3 HDRColor = DoPbrLighting(Input, myPerFrame, diffuseColor, specularColor, perceptualRoughness,metallic);
    Output.Target0 = float4(HDRColor, alpha);
    Output.Target1 = float4(Calculate3DVelocity(Input.svCurrPosition, Input.svPrevPosition),0); 
    Output.Target2 = float4(getPixelNormal(Input) / 2 + 0.5f, 0);
    Output.Target3 = EmissMap.Sample(SampleLinear, Input.UV0);
    Output.Target4 = float4(metallic, 0.5, perceptualRoughness, 1.0);
    return Output;
}