// Debug / tooling passes only. Sky + IBL offline bake live in EnvironmentSkyIBL.hlsl (single cube @ t0).
#ifndef EnvironmentShaders
#define EnvironmentShaders

#include "EnvironmentCommon.hlsl"

typedef EnvironmentVertexIN VertexIN;
typedef EnvironmentVertexOut VertexOutput;

TextureCube CubeEnvironment : register(t0);
Texture2D InputTexture : register(t1);

//-------------------------------------------------------
// Show Texture2D
//-------------------------------------------------------

struct VertexOutput_Texture2D
{
    float4 Pos : SV_Position;
    float2 Tex : TEXCOORD;
};

VertexOutput_Texture2D VS_ShowTexture2D(in uint VertID : SV_VertexID)
{
    VertexOutput_Texture2D Output;
    float2 Tex = float2(uint2(VertID, VertID << 1) & 2);
    Output.Tex = Tex;
    Output.Pos = float4(lerp(float2(-1, 1), float2(1, -1), Tex), 0, 1);
    return Output;
}

float4 PS_ShowTexture2D(in VertexOutput_Texture2D In) : SV_Target0
{
    float3 Color = InputTexture.Sample(LinearSampler, In.Tex).xyz;
    return float4(ToneMapping(Color * Exposure), 1.0);
}

float4 PS_ShowTexture2DNormal(in VertexOutput_Texture2D In) : SV_Target0
{
    float3 Color = InputTexture.Sample(LinearSampler, In.Tex).xyz;
    return float4(Color, 1.0);
}

//-------------------------------------------------------
// CubeMap Cross View
//-------------------------------------------------------

struct VertexIN_CubeMapCross
{
    float3 Position : ATTRIBUTE0;
    float3 Normal : ATTRIBUTE1;
};

VertexOutput VS_CubeMapCross(VertexIN_CubeMapCross In)
{
    VertexOutput Out;
    Out.LocalDirection = In.Normal;
    Out.Position = mul(mul(float4(In.Position, 1.0), GetWorldMatrix()), GetCameraViewProj());
    return Out;
}

float4 PS_CubeMapCross(VertexOutput In) : SV_Target
{
    float3 Color = CubeEnvironment.SampleLevel(LinearSampler, In.LocalDirection, MipLevel).xyz;
    return float4(ToneMapping(Color * Exposure), 1.0);
}

#endif
