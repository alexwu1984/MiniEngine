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
   // Out.Position.z = Out.Position.w;
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

Texture2D LongLatEnvironment : register(t0);
//-------------------------------------------------------
// Convert Longtitude-Latitude Mapping to Cube Mapping
//-------------------------------------------------------

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
    return LongLatEnvironment.Sample(LinearSampler, SampleSphericalMap(In.LocalDirection.xyz));
}

//-------------------------------------------------------
// Generate Prefiltered map
//-------------------------------------------------------

// VS is same as VS_SkyCube

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
            //https://placeholderart.wordpress.com/2015/07/28/implementation-notes-runtime-environment-map-filtering-for-image-based-lighting/
            //float PDF = D_GGX( Pow4(Roughness), NoH ) * NoH / (4 * VoH);  //NoH == VoH
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

float4 PS_GenPrefiltered(VertexOutput In, float4 SvPosition : SV_POSITION) : SV_Target
{
    int2 PixelPos = int2(SvPosition.xy);
    uint2 Random = Rand3DPCG16(uint3(PixelPos, In.Position.x * 1024)).xy;

    float3 R = normalize(In.LocalDirection);
    //float Roughness = MipLevel / (MaxMipLevel - 1.0);
    float Roughness = ComputeReflectionCaptureRoughnessFromMip(MipLevel, MaxMipLevel - 1.0);
    float3 Prefiltered = PrefilterEnvMap(Random, Roughness, R);
    return float4(Prefiltered, 1.0);
}