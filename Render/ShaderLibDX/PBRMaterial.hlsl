// Do NOT include EnvironmentSkyIBL.hlsl / EnvironmentShaders.hlsl: they bind a cubemap at t0 and collide with PBR t0–t8.
#include "ShaderUtils.hlsl"
#include "GLTFPbrPass-VS.hlsl"
#include "GLTFPbrPass-IO.hlsl"

// D3D12: five material maps as one descriptor range (ps_5_1). D3D11: same file, no RHI_BINDLESS macro.
#if defined(RHI_BINDLESS)
Texture2D PBR_Material2D[5] : register(t0);
#define AlbedoMap PBR_Material2D[0]
#define NormalMap PBR_Material2D[1]
#define Roughness_metallicMap PBR_Material2D[2]
#define EmissMap PBR_Material2D[3]
#define AoMap PBR_Material2D[4]
#else
Texture2D AlbedoMap : register(t0);
Texture2D NormalMap : register(t1);
Texture2D Roughness_metallicMap : register(t2);
Texture2D EmissMap : register(t3);
Texture2D AoMap : register(t4);
#endif
SamplerState SampleLinear : register(s0);

struct PS_OUTPUT_SCENE
{
    float4 Target0 : SV_Target0; //Scene color
    float4 Target1 : SV_Target1; //Velocity buffer
    float4 Target2 : SV_Target2; //Normal
    float4 Target3 : SV_Target3; //Emissive
    float4 Target4 : SV_Target4; //metallSpecularRoughness
};

float3 getNormalTexture(VS_OUTPUT_SCENE Input)
{
    float2 xy = 2.0 * NormalMap.SampleBias(SampleLinear, Input.UV0, myPerFrame.LodBias).rg - 1.0;
    float z = sqrt(1.0f - dot(xy, xy));
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
    float3 t = (tex_dy.y * pos_dx - tex_dx.y * pos_dy) / (tex_dx.x * tex_dy.y - tex_dy.x * tex_dx.y);
    float3 ng = normalize(Input.Normal);

    t = normalize(t - ng * dot(ng, t));
    float3 b = normalize(cross(ng, t));
    float3x3 tbn = float3x3(t, b, ng);
#else // HAS_TANGENTS
    float3x3 tbn = float3x3(Input.Tangent, Input.Binormal, Input.Normal);
#endif

    float3 n = getNormalTexture(Input);
    n = normalize(mul((n /* * float3(u_NormalScale, u_NormalScale, 1.0) */), tbn));

    return n * (bIsFontFacing ? -1 : 1);
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


PS_OUTPUT_SCENE MainPS(VS_OUTPUT_SCENE Input)
{
	PS_OUTPUT_SCENE Output;
	Output.Target0 = float4(0.0, 0.0, 0.0, 0.0);
	Output.Target1 = float4(0.0, 0.0, 0.0, 0.0);
	Output.Target2 = float4(0.0, 0.0, 0.0, 0.0);
	Output.Target3 = float4(0.0, 0.0, 0.0, 0.0);
	Output.Target4 = float4(0.0, 0.0, 0.0, 0.0);

    float alpha;
    float perceptualRoughness;
    float3 diffuseColor;
    float3 specularColor;
    float metallic;
    GetPBRParams(Input, diffuseColor, specularColor, perceptualRoughness, metallic, alpha);
    float ao = AoMap.Sample(SampleLinear, Input.UV0).r;

    Output.Target1 = float4(Calculate3DVelocity(Input.svCurrPosition, Input.svPrevPosition), 0);
    Output.Target2 = float4(getPixelNormal(Input) / 2 + 0.5f, 0);
    Output.Target3 = EmissMap.Sample(SampleLinear, Input.UV0);
    Output.Target4 = float4(metallic, ao, perceptualRoughness, 1.0);

    // Unlit: final color in SceneColor (deferred pass skipped on CPU). Still fill GBuffer for effects that read normals/MR.
    if (myPerFrame.bUnlit != 0)
    {
        float4 bc = AlbedoMap.Sample(SampleLinear, Input.UV0);
        float3 albedoLin = sRGBToLinear(bc.rgb);
        float3 emLin = sRGBToLinear(EmissMap.Sample(SampleLinear, Input.UV0).rgb);
        Output.Target0 = float4(albedoLin + emLin, bc.a);
        return Output;
    }

    // Lit: G-buffer only; analytic + IBL lighting in DeferredLighting.hlsl (Target0 = linear base albedo, not shaded HDR).
    float4 baseTex = AlbedoMap.Sample(SampleLinear, Input.UV0);
    Output.Target0 = float4(sRGBToLinear(baseTex.rgb), alpha);
    return Output;
}