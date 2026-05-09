// Longitude–latitude HDR → cubemap face capture only.
// Kept separate from EnvironmentSkyIBL.hlsl (TextureCube @ t0) so this file keeps 2D @ t0 only (GBV #940).

#include "ShaderUtils.hlsl"
#include "PerFrameStruct.hlsl"

struct VertexIN
{
    float3 Position : ATTRIBUTE0;
};

struct VertexOutput
{
    float4 Position : SV_Position;
    float3 LocalDirection : TEXCOORD0;
    float4 HClip : TEXCOORD1;
};

SamplerState LinearSampler : register(s0);
Texture2D LongLatEnvironment : register(t0);

VertexOutput VS_SkyCube(VertexIN In)
{
    VertexOutput Out;
    Out.LocalDirection = In.Position;
    float4 h = mul(mul(float4(In.Position, 1.0), GetWorldMatrix()), GetCameraViewProj());
    Out.HClip = h;
    Out.Position = h;
    return Out;
}

static const float2 invAtan = { 0.5 / PI, -1 / PI };

float2 SampleSphericalMap(float3 Direction)
{
    float3 v = normalize(Direction);
    float2 uv = { atan2(v.z, v.x), asin(v.y) };
    uv = saturate(uv * invAtan + 0.5);
    return uv;
}

float4 PS_LongLatToCube(VertexOutput In) : SV_Target
{
    float3 dir = SkyCubeDirectionFromHClip(In.HClip);
    return LongLatEnvironment.Sample(LinearSampler, SampleSphericalMap(dir));
}
