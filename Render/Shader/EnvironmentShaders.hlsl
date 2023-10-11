#include "ShaderUtils.hlsl"
#include "PerFrameStruct.hlsl"

#ifndef EnvironmentShaders
#define EnvironmentShaders

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


float3 ACESToneMapping(float3 Color)
{
    return LinearToSrgb(ACESFilm(Color));
}

//--------------------------------------------------------------------------------------
// AMD Tonemapper
//--------------------------------------------------------------------------------------
// General tonemapping operator, build 'b' term.
float ColToneB(float hdrMax, float contrast, float shoulder, float midIn, float midOut)
{
    return
        -((-pow(midIn, contrast) + (midOut * (pow(hdrMax, contrast * shoulder) * pow(midIn, contrast) -
            pow(hdrMax, contrast) * pow(midIn, contrast * shoulder) * midOut)) /
            (pow(hdrMax, contrast * shoulder) * midOut - pow(midIn, contrast * shoulder) * midOut)) /
            (pow(midIn, contrast * shoulder) * midOut));
}

// General tonemapping operator, build 'c' term.
float ColToneC(float hdrMax, float contrast, float shoulder, float midIn, float midOut)
{
    return (pow(hdrMax, contrast * shoulder) * pow(midIn, contrast) - pow(hdrMax, contrast) * pow(midIn, contrast * shoulder) * midOut) /
           (pow(hdrMax, contrast * shoulder) * midOut - pow(midIn, contrast * shoulder) * midOut);
}

// General tonemapping operator, p := {contrast,shoulder,b,c}.
float ColTone(float x, float4 p)
{
    float z = pow(x, p.r);
    return z / (pow(z, p.g) * p.b + p.a);
}

float3 AMDTonemapper(float3 color)
{
    static float hdrMax = 16.0; // How much HDR range before clipping. HDR modes likely need this pushed up to say 25.0.
    static float contrast = 2.0f; // Use as a baseline to tune the amount of contrast the tonemapper has.
    static float shoulder = 1.0; // Likely don¡¯t need to mess with this factor, unless matching existing tonemapper is not working well..
    static float midIn = 0.18; // most games will have a {0.0 to 1.0} range for LDR so midIn should be 0.18.
    static float midOut = 0.18; // Use for LDR. For HDR10 10:10:10:2 use maybe 0.18/25.0 to start. For scRGB, I forget what a good starting point is, need to re-calculate.

    float b = ColToneB(hdrMax, contrast, shoulder, midIn, midOut);
    float c = ColToneC(hdrMax, contrast, shoulder, midIn, midOut);

#define EPS 1e-6f
    float peak = max(color.r, max(color.g, color.b));
    peak = max(EPS, peak);

    float3 ratio = color / peak;
    peak = ColTone(peak, float4(contrast, shoulder, b, c));
    // then process ratio

    // probably want send these pre-computed (so send over saturation/crossSaturation as a constant)
    float crosstalk = 4.0; // controls amount of channel crosstalk
    float saturation = 1.5f; // full tonal range saturation control
    float crossSaturation = contrast * 16.0; // crosstalk saturation

    float white = 1.0;

    // wrap crosstalk in transform
    ratio = pow(abs(ratio), saturation / crossSaturation);
    ratio = lerp(ratio, white, pow(peak, crosstalk));
    ratio = pow(abs(ratio), crossSaturation);

    // then apply ratio to peak
    color = peak * ratio;
    return LinearToSrgb(color);
}

#endif