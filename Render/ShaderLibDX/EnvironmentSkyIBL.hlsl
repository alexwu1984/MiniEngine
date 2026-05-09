// Sky + offline IBL bake only: single TextureCube @ t0 per compilation unit (clean D3D12 reflection / GBV).
#ifndef EnvironmentSkyIBL_HLSL
#define EnvironmentSkyIBL_HLSL

#include "EnvironmentCommon.hlsl"

TextureCube CubeEnvironment : register(t0);

EnvironmentVertexOut VS_SkyCube(EnvironmentVertexIN In)
{
    EnvironmentVertexOut Out;
    Out.LocalDirection = In.Position;
    float4 h = mul(mul(float4(In.Position, 1.0), GetWorldMatrix()), GetCameraViewProj());
    Out.HClip = h;
    Out.Position = h;
    return Out;
}

float4 PS_SkyCube(EnvironmentVertexOut In) : SV_Target
{
    float3 dir = SkyCubeDirectionFromHClip(In.HClip);
    float4 Sample = CubeEnvironment.Sample(LinearSampler, dir);
    return float4(Sample.xyz * Exposure, 1.0);
}

float4 PS_GenIrradiance(EnvironmentVertexOut In) : SV_Target
{
    float3 Normal = SkyCubeDirectionFromHClip(In.HClip);
    float3 Irradiance = { 0.0, 0.0, 0.0 };

    float3 Up = { 0.0, 1.0, 0.0 };
    float3 Right = cross(Up, Normal);
    Up = cross(Normal, Right);

    float sampleDelta = 1.0 / NumSamplesPerDir;

    uint2 Dimension;
    CubeEnvironment.GetDimensions(Dimension.x, Dimension.y);
    float lod = max(log2(Dimension.x / float(NumSamplesPerDir)) + 1.0, 0.0);

    float NumSamples = 0.0;
    for (float phi = 0.0; phi < 2.0 * PI; phi += sampleDelta)
    {
        for (float theta = 0.0; theta < 0.5 * PI; theta += sampleDelta)
        {
            float sintheta = sin(theta);
            float costheta = cos(theta);
            float3 tangentSample = float3(sintheta * cos(phi), sintheta * sin(phi), costheta);
            float3 sampleVec = TangentToWorld(tangentSample, Normal);
            float3 sampleColor = CubeEnvironment.SampleLevel(LinearSampler, sampleVec, lod).rgb;

            Irradiance += sampleColor * costheta * sintheta;
            NumSamples++;
        }
    }

    Irradiance = PI * Irradiance / NumSamples;
    return float4(Irradiance, 1.0);
}

float3 PrefilterEnvMap(uint2 Random, float Roughness, float3 R)
{
    float3 FilteredColor = 0;
    float Weight = 0;

    const float K = 2.0;
    uint CubeSize = 1 << (MaxMipLevel - 1);
    const float SolidAngleTexel = 4 * PI / (6 * CubeSize * CubeSize);

    const uint NumSamples = Roughness < 0.1 ? 32 : 64;
    for (uint i = 0; i < NumSamples; i++)
    {
        float2 E = Hammersley(i, NumSamples, 0);
        float3 H = TangentToWorld(ImportanceSampleGGX(E, Pow4(Roughness)).xyz, R);
        float3 L = 2 * dot(R, H) * H - R;

        float NoL = saturate(dot(R, L));
        float NoH = saturate(dot(R, H));
        if (NoL > 0)
        {
            float PDF = D_GGX(Pow4(Roughness), NoH) * 0.25;
            float SolidAngleSample = 1.0 / (NumSamples * PDF);
            float MipBias = 1.0;
            float Mip = Roughness == 0 ? 0 : clamp(0.5 * log2(K * SolidAngleSample / SolidAngleTexel) + MipBias, 0, MaxMipLevel - 1);

            FilteredColor += CubeEnvironment.SampleLevel(LinearSampler, L, Mip).rgb * NoL;
            Weight += NoL;
        }
    }

    return FilteredColor / max(Weight, 0.001);
}

float4 PS_GenPrefiltered(EnvironmentVertexOut In, float4 SvPosition : SV_POSITION) : SV_Target
{
    int2 PixelPos = int2(SvPosition.xy);
    uint2 Random = Rand3DPCG16(uint3(PixelPos, In.Position.x * 1024)).xy;

    float3 R = SkyCubeDirectionFromHClip(In.HClip);
    float Roughness = ComputeReflectionCaptureRoughnessFromMip(MipLevel, MaxMipLevel - 1.0);
    float3 Prefiltered = PrefilterEnvMap(Random, Roughness, R);
    return float4(Prefiltered, 1.0);
}

#endif // EnvironmentSkyIBL_HLSL
