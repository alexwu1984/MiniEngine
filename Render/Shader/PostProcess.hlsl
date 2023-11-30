#include "ShaderUtils.hlsl"

struct VertexOutput
{
    float2 Tex : TEXCOORD;
    float4 Pos : SV_Position;
};

Texture2D SceneColorTexture : register(t0);
SamplerState LinearSampler : register(s0);

VertexOutput VS_ScreenQuad(in uint VertID : SV_VertexID)
{
    VertexOutput Output;
    // Texture coordinates range [0, 2], but only [0, 1] appears on screen.
    float2 Tex = float2(uint2(VertID, VertID << 1) & 2);
    Output.Tex = Tex;
    Output.Pos = float4(lerp(float2(-1, 1), float2(1, -1), Tex), 0, 1);
    return Output;
}

////-------------------------------------------------------
//// Simple post processing shader 
//// only tonemap and gamma
////-------------------------------------------------------
float4 PS_Tonemapping(in VertexOutput Input) : SV_Target0
{
    float3 Color = SceneColorTexture.Sample(LinearSampler, Input.Tex).xyz;
    return float4(AMDTonemapping(Color), 1.0);
}

static float2 offsets[9] =
{
    float2(1, 1), float2(0, 1), float2(-1, 1),
    float2(1, 0), float2(0, 0), float2(-1, 0),
    float2(1, -1), float2(0, -1), float2(-1, -1)
};

cbuffer DownSampleParam : register(b0)
{
    float2 u_invSize;
    int u_mipLevel;
    int pad;
};

////----downsample
float4 PS_DownSample(in VertexOutput Input) : SV_Target0
{
    // gaussian like downsampling
    
    float4 color = float4(0, 0, 0, 0);

    if (u_mipLevel == 0)
    {
        for (int i = 0; i < 9; i++)
            color += log(max(SceneColorTexture.Sample(LinearSampler, Input.Tex + (2 * u_invSize * offsets[i])), float4(0.01, 0.01, 0.01, 0.01)));
        return exp(color / 9.0f);
    }
    else
    {
        for (int i = 0; i < 9; i++)
            color += SceneColorTexture.Sample(LinearSampler, Input.Tex + (2 * u_invSize * offsets[i]));
        return color / 9.0f;
    }
}