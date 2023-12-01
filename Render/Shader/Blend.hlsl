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

cbuffer CBBlendParam : register(b0)
{
    float u_weight;
}

//--------------------------------------------------------------------------------------
// Main function
//--------------------------------------------------------------------------------------

float4 PS_Main(VertexOutput Input) : SV_Target
{
    return u_weight * SceneColorTexture.Sample(LinearSampler, Input.Tex).rgba;
}
