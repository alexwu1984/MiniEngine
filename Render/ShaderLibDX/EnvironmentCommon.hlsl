// Shared sky / IBL / debug environment declarations (no TextureCube / Texture2D SRVs here — one TU per resource layout).
#ifndef EnvironmentCommon_HLSL
#define EnvironmentCommon_HLSL

#include "ShaderUtils.hlsl"
#include "PerFrameStruct.hlsl"

struct EnvironmentVertexIN
{
    float3 Position : ATTRIBUTE0;
};

struct EnvironmentVertexOut
{
    float4 Position : SV_Position;
    float3 LocalDirection : TEXCOORD0;
    float4 HClip : TEXCOORD1;
};

cbuffer PSContant : register(b5)
{
    float Exposure;
    int MipLevel;
    int MaxMipLevel;
    int NumSamplesPerDir;
};

SamplerState LinearSampler : register(s0);

#endif // EnvironmentCommon_HLSL
