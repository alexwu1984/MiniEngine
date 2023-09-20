#include "ShaderUtils.hlsl"
#include "PerFrameStruct.hlsl"

struct VertexIN
{
    float3 Position : ATTRIBUTE0;
};

struct VertexOutput
{
    float4 Position : SV_Position;
    float3 LocalDirection : TEXCOORD;
};

cbuffer PSContant : register(b5)
{
    float Exposure;
    int MipLevel;
    int MaxMipLevel;
    int NumSamplesPerDir;
};

SamplerState LinearSampler : register(s0);
TextureCube CubeEnvironment : register(t0);
//--------------------------------------------------------------------------------------
// SkyBox
//--------------------------------------------------------------------------------------
VertexOutput VS_SkyCube(VertexIN In)
{
    VertexOutput Out;
    Out.LocalDirection = In.Position;
    Out.Position = mul(mul(float4(In.Position, 1.0), GetWorldMatrix()), GetCameraViewProj());
    Out.Position.z = Out.Position.w;
    return Out;
}

float4 PS_GenIrradiance(VertexOutput In) : SV_Target
{
	//return CubeEnvironment.Sample(LinearSampler, In.LocalDirection);
    float3 Normal = normalize(In.LocalDirection);
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
			// spherical to cartesian (in tangent space)
            float sintheta = sin(theta);
            float costheta = cos(theta);
            float3 tangentSample = float3(sintheta * cos(phi), sintheta * sin(phi), costheta);
			// tangent space to world
			//float3 sampleVec = tangentSample.x * Right + tangentSample.y * Up + tangentSample.z * Normal;
            float3 sampleVec = TangentToWorld(tangentSample, Normal);
            float3 sampleColor = CubeEnvironment.SampleLevel(LinearSampler, sampleVec, lod).rgb;

            Irradiance += sampleColor * costheta * sintheta;
            NumSamples++;
        }
    }

    Irradiance = PI * Irradiance / NumSamples;

    return float4(Irradiance, 1.0);
}

Texture2D EquirectangularMap : register(t0);

float4 EVN_PS(VertexOutput In) : SV_Target
{
    float3 v = normalize(In.LocalDirection);
    float2 uv = float2(atan2(v.z, v.x), asin(v.y));
    uv.x *= 0.1591;
    uv.y *= 0.3183;
    uv += 0.5;
    float3 color = EquirectangularMap.Sample(LinearSampler, uv).rgb;
    return float4(color, 1.0);
}
