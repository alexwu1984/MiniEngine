#include "GLTFPbrPass-VS.hlsl"
#include "GLTFPbrPass-IO.hlsl"


Texture2D AlbedoMap : register(t0);
Texture2D NoiseMap : register(t1);
SamplerState SampleLinear : register(s0);

struct PS_OUTPUT_SCENE
{
	float4 Color : SV_Target0;
};

PS_OUTPUT_SCENE MainPS(VS_OUTPUT_SCENE Input) : SV_Target
{
	PS_OUTPUT_SCENE Output;
    
    float3 BaseColor = AlbedoMap.Sample(SampleLinear, Input.UV1).rgb;
    if(DrawSolid.x == 1)
    {
        Output.Color = float4(Output.Color.rgb, 1.f);
        return Output;
    }
    
    float Noise = 1.0;
    Noise = NoiseMap.Sample(SampleLinear, Input.UV0).r;

    //Ambient occlusion
    float Occlusion = FurOffset * FurOffset;
    Occlusion += 0.04;
    float3 SHL = lerp(FurColor * Input.SH, Input.SH, Occlusion);
    
    float3 LightDir = float3(0.f, 0.f, 1.f);
  
    //太阳光
    float3 L = normalize(LightDir.xyz);
    float NoL = dot(L, normalize(Input.Normal));
    float LightFilter = 1.6f;
    float DirLight = clamp(NoL + LightFilter + FurOffset, 0.0, 1.0);
    
    //轮廓光
    float FresnelLV = 2.0f;
    float3 V = normalize(myPerFrame.CameraPos.xyz - Input.WorldPos);
    float Fresnel = 1.0 - max(0.0, dot(Input.Normal, V));
    float3 RimLight = float3(Fresnel * Occlusion, Fresnel * Occlusion, Fresnel * Occlusion); //这个值会很小，因为Occlusion太小了，所以对最终效果影响比较小
    RimLight *= RimLight;
    RimLight *= FresnelLV * Input.SH * BaseColor;
    SHL += RimLight;
    
    float FurMask = 0.5;
    float Tming = 0.5;
    float Alpha = clamp((Noise * 2.0 - (FurOffset * FurOffset + (FurOffset * FurMask * 5.0))) * Tming, 0.0, 1.0);

    float3 OutColor = Output.Color.rgb * FurLightExposure * FurAmbientStrength + SHL * FurLightExposure;

    Output.Color = float4(Output.Color.rgb, Alpha);
    return Output;
}
