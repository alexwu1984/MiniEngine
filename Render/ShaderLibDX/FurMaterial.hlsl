#include "GLTFPbrPass-VS.hlsl"
#include "GLTFPbrPass-IO.hlsl"
#include "PerFrameStruct.hlsl"
#include "ShaderUtils.hlsl"
#include "DeferredShadingCommon.hlsl"
#include "HairShading.hlsl"

#if defined(RHI_BINDLESS)
Texture2D Fur_Material2D[2] : register(t0);
#define AlbedoMap Fur_Material2D[0]
#define NoiseMap  Fur_Material2D[1]
#else
Texture2D AlbedoMap : register(t0);
Texture2D NoiseMap : register(t1);
#endif
SamplerState SampleLinear : register(s0);

struct PS_OUTPUT_SCENE
{
    float4 Target0 : SV_Target0;
    float4 Target1 : SV_Target1;
    float4 Target2 : SV_Target2;
    float4 Target3 : SV_Target3;
    float4 Target4 : SV_Target4;
    float4 Target5 : SV_Target5;
};

float3 Calculate3DFurVelocity(float4 CurrentVelocity, float4 PreVelocity)
{
    float2 ScreenPos = CurrentVelocity.xy / CurrentVelocity.w - myPerFrame.TemporalAAJitter.xy;
    float2 PrevScreenPos = PreVelocity.xy / PreVelocity.w - myPerFrame.TemporalAAJitter.zw;
    float DeviceZ = CurrentVelocity.z / CurrentVelocity.w;
    float PrevDeviceZ = PreVelocity.z / PreVelocity.w;
    return float3(ScreenPos - PrevScreenPos, DeviceZ - PrevDeviceZ);
}

PS_OUTPUT_SCENE MainPS(VS_OUTPUT_SCENE Input)
{
    PS_OUTPUT_SCENE Output = (PS_OUTPUT_SCENE)0;

    float3 BaseColor = sRGBToLinear(AlbedoMap.Sample(SampleLinear, Input.UV1).rgb);
    float3 n = normalize(Input.Normal);
    float3 nPacked = n * 0.5 + 0.5;
    float3 FurVelocity = Calculate3DFurVelocity(Input.svCurrPosition, Input.svPrevPosition);

    // Defaults for fur: dielectric, full AO, medium roughness (shells refine alpha only).
    const float kMetallic = 0.0;
    const float kAO = 1.0;
    const float kRough = 0.85;

    if (DrawSolid.x == 1)
    {
        // Prepass: write depth, but keep MRT color unchanged (alpha=0 under BlendDeferredTranslucentMRT).
        Output.Target1 = float4(0, 0, 0, 0);
        Output.Target2 = float4(0, 0, 0, 0);
        Output.Target0 = float4(0, 0, 0, 0);
        Output.Target3 = float4(0, 0, 0, 0);
        Output.Target4 = float4(0, 0, 0, 0);
        Output.Target5 = float4(0, 0, 0, 0);
        return Output;
    }

    float Noise = NoiseMap.Sample(SampleLinear, Input.UV0).r;
    float Occlusion = FurOffset * FurOffset;
    Occlusion += 0.04;

    float FurMask = 0.5;
    float Tming = 0.5;
    float Alpha = clamp((Noise * 2.0 - (FurOffset * FurOffset + (FurOffset * FurMask * 5.0))) * Tming, 0.0, 1.0);

    // BlendTraditional only blended RT0; RT1–RT4 defaulted to replace so emissive/MR/normals ignored strand Alpha — fix by matching Alpha on every target (BlendDeferredTranslucentMRT).
    Output.Target1 = float4(FurVelocity, Alpha);
    Output.Target2 = float4(nPacked, Alpha);

    // Shell rim into emissive (after deferred analytic + IBL): Fresnel × vertex SH shaping × exposure — restores silhouette fluff.
    float3 V = normalize(myPerFrame.CameraPos.xyz - Input.WorldPos);
    float Fresnel = 1.0 - max(0.0, dot(Input.Normal, V));
    float3 RimLight = float3(Fresnel * Occlusion, Fresnel * Occlusion, Fresnel * Occlusion);
    RimLight *= RimLight;
    RimLight *= 2.0 * Input.SH * BaseColor * FurAmbientStrength;
    float3 ExtraEmissive = RimLight * FurLightExposure;
    // Linear base color into GBuffer like lit PBR (DeferredLighting applies diffuse/spec/IBL); avoid extra ambient pre-multiply on albedo.
    float3 ShellAlbedo = BaseColor * FurLightExposure;

    Output.Target0 = float4(ShellAlbedo, Alpha);
    Output.Target3 = float4(ExtraEmissive, Alpha);
    Output.Target4 = float4(kMetallic, kAO, kRough, Alpha);
    // SHADINGMODELID_HAIR + strand tangent oct (MaterialAux .a is coverage for MRT blend; IBL hair scale fixed in deferred).
    float3 strandDir;
#if defined(HAS_TANGENT)
    strandDir = normalize(Input.Tangent);
#else
    float3 up = float3(0.0, 1.0, 0.0);
    strandDir = cross(n, up);
    strandDir = (dot(strandDir, strandDir) > 1e-8) ? normalize(strandDir) : normalize(cross(n, float3(1.0, 0.0, 0.0)));
#endif
    float2 tOct = EncodeHairTangentOctPacked(strandDir);
    float4 auxHair = EncodeMaterialAux_HairStrand(tOct, 1.0);
    Output.Target5 = float4(auxHair.r, auxHair.g, auxHair.b, Alpha);

    if (myPerFrame.bUnlit != 0)
    {
        float3 em = Output.Target3.rgb;
        Output.Target0 = float4(ShellAlbedo + em, Alpha);
        Output.Target3 = float4(0, 0, 0, 0);
    }

    return Output;
}
