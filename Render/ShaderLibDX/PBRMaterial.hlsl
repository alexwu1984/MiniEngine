// Do NOT include EnvironmentSkyIBL.hlsl / EnvironmentShaders.hlsl: they bind a cubemap at t0 and collide with PBR t0–t8.
#include "ShaderUtils.hlsl"
#include "GLTFPbrPass-VS.hlsl"
#include "PBRMaterialSampling.hlsl"

struct PS_OUTPUT_SCENE
{
    float4 Target0 : SV_Target0; //Scene color
    float4 Target1 : SV_Target1; //Velocity buffer
    float4 Target2 : SV_Target2; //Normal
    float4 Target3 : SV_Target3; //Emissive
    float4 Target4 : SV_Target4; //metallSpecularRoughness
};

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

    if (myMaterial.AlphaMask != 0)
        clip(alpha - myMaterial.AlphaCutoff);

    // AO may live in any channel depending on source asset; take max so wrong swizzle / packed maps do not zero IBL.
    float4 aoSamp = AoMap.Sample(SampleLinear, Input.UV0);
    float ao = max(max(aoSamp.r, aoSamp.g), aoSamp.b);

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
#if defined(WRITE_BASECOLOR_ALPHA_TO_GBUFFER)
        Output.Target0 = float4(albedoLin + emLin, bc.a);
#else
        Output.Target0 = float4(albedoLin + emLin, 1.0);
#endif
        return Output;
    }

    // Lit: G-buffer only; analytic + IBL lighting in DeferredLighting.hlsl (Target0 = linear base albedo, not shaded HDR).
    float4 baseTex = AlbedoMap.Sample(SampleLinear, Input.UV0);
#if defined(WRITE_BASECOLOR_ALPHA_TO_GBUFFER)
    Output.Target0 = float4(sRGBToLinear(baseTex.rgb), alpha);
#else
    Output.Target0 = float4(sRGBToLinear(baseTex.rgb), 1.0);
#endif
    return Output;
}