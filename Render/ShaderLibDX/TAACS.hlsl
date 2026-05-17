#include "ShaderUtils.hlsl"

RWTexture2D<float4> OutTemporal             : register(u0);
Texture2D<float4> InColor                   : register(t0);
Texture2D<float4> InTemporal                : register(t1);
Texture2D<float4> VelocityBuffer            : register(t2);
Texture2D<float> DepthBuffer                : register(t3);

SamplerState MinMagLinearMipPointClamp      : register(s0);

cbuffer CB0 : register(b0)
{
    float4  Resolution;                     // width, height, 1/width, 1/height
    int     FrameIndex;
    float3  Pad0;
    float4  CurrentJitterPixels;
}

#define SPATIAL_WEIGHT_CATMULLROM 1
#define LONGEST_VELOCITY_VECTOR_SAMPLES 0

// Frostbite/UE4 (Karis): fixed feedback, variance clip in YCoCg, HDR-weighted accumulation.
static const float Exposure = 10.0f;
static const float Feedback = 0.04f;
static const float VarianceGamma = 1.0f;
static const float2 kVelocityRefResolution = float2(1920.0f, 1080.0f);
static const float kVelocityRejectPixelsAtRef = 128.0f;

// UE reactive mask: reduce current-frame weight on bright highlights (less firefly / specular pop).
static const float ReactiveLumaThreshold = 0.45f;
static const float ReactiveLumaScale = 6.0f;
static const float ReactiveBlendSub = 0.28f;
static const float ReactiveBlendMin = 0.02f;

static const int2 SampleOffsets[9] =
{
    int2(-1, -1), int2(0, -1), int2(1, -1),
    int2(-1, 0),  int2(0, 0),  int2(1, 0),
    int2(-1, 1),  int2(0, 1),  int2(1, 1),
};

float Luma4(float3 Color)
{
    return Color.r;
}

float HdrWeight4(float3 Color, float ExposureValue)
{
    const float luma = max(Luma4(Color), 0.0f);
    return rcp(luma * ExposureValue + 4.0f);
}

float3 TaaToneCurveFwd(float3 color)
{
    return ACESFilm(max(color, 0.0f));
}

float3 TaaToneCurveInv(float3 color)
{
    return InverseACESFilmLinear(saturate(color));
}

float3 RGBToYCoCg(float3 RGB)
{
    const float Y = dot(RGB, float3(1, 2, 1));
    const float Co = dot(RGB, float3(2, 0, -2));
    const float Cg = dot(RGB, float3(-1, 2, -1));
    return float3(Y, Co, Cg);
}

float3 YCoCgToRGB(float3 YCoCg)
{
    const float Y = YCoCg.x * 0.25f;
    const float Co = YCoCg.y * 0.25f;
    const float Cg = YCoCg.z * 0.25f;
    return float3(Y + Co - Cg, Y + Cg, Y - Co - Cg);
}

float Lanczos2(float x)
{
    const float x2 = x * x;
    float wA = x2 - 4.0f;
    float wB = x2 * wA - wA;
    wA *= wA;
    wB *= wA;
    return wB * (1.0f / 64.0f);
}

float2 GetUV(float2 inST)
{
    return (inST + 0.5f.xx) * Resolution.zw;
}

float3 GetCurrentColour(float2 screenST)
{
    const float2 uv = GetUV(screenST);
    float3 colour = InColor.SampleLevel(MinMagLinearMipPointClamp, uv, 0).rgb;
    colour = TaaToneCurveFwd(colour);
    return RGBToYCoCg(colour);
}

float3 SampleHistory(float2 inHistoryST)
{
    const float2 historyScreenUV = GetUV(inHistoryST);

    float2 samplePos = historyScreenUV * Resolution.xy;
    float2 texPos1 = floor(samplePos - 0.5f.xx) + 0.5f.xx;
    float2 f = samplePos - texPos1;
    float2 f2 = f * f;
    float2 f3 = f2 * f;

    float2 w0 = f2 - 0.5f * (f3 + f);
    float2 w1 = 1.5f * f3 - 2.5f * f2 + 1.0f;
    float2 w3 = 0.5f * (f3 - f2);
    float2 w2 = 1.0f - w0 - w1 - w3;
    float2 w12 = w1 + w2;

    float2 texPos0 = texPos1 - 1.0f.xx;
    float2 texPos3 = texPos1 + 2.0f.xx;
    float2 texPos12 = texPos1 + w2 / w12;

    texPos0 *= Resolution.zw;
    texPos3 *= Resolution.zw;
    texPos12 *= Resolution.zw;

    const float k0 = w12.x * w0.y;
    const float k1 = w0.x * w12.y;
    const float k2 = w12.x * w12.y;
    const float k3 = w3.x * w12.y;
    const float k4 = w12.x * w3.y;

    float3 s0 = InTemporal.SampleLevel(MinMagLinearMipPointClamp, float2(texPos12.x, texPos0.y), 0).rgb;
    float3 s1 = InTemporal.SampleLevel(MinMagLinearMipPointClamp, float2(texPos0.x, texPos12.y), 0).rgb;
    float3 s2 = InTemporal.SampleLevel(MinMagLinearMipPointClamp, float2(texPos12.x, texPos12.y), 0).rgb;
    float3 s3 = InTemporal.SampleLevel(MinMagLinearMipPointClamp, float2(texPos3.x, texPos12.y), 0).rgb;
    float3 s4 = InTemporal.SampleLevel(MinMagLinearMipPointClamp, float2(texPos12.x, texPos3.y), 0).rgb;

    float3 history = (k0 * s0 + k1 * s1 + k2 * s2 + k3 * s3 + k4 * s4) * rcp(max(k0 + k1 + k2 + k3 + k4, 0.0001f));
    const float3 boxMin = min(min(min(s0, s1), min(s2, s3)), s4);
    const float3 boxMax = max(max(max(s0, s1), max(s2, s3)), s4);
    history = clamp(history, boxMin, boxMax);

    history = TaaToneCurveFwd(history);
    return RGBToYCoCg(history);
}

float HistoryClip(float3 History, float3 Filtered, float3 NeighborMin, float3 NeighborMax)
{
    float3 RayOrigin = History;
    float3 RayDir = Filtered - History;
    RayDir.x = abs(RayDir.x) < (1.0f / 65536.0f) ? (1.0f / 65536.0f) : RayDir.x;
    RayDir.y = abs(RayDir.y) < (1.0f / 65536.0f) ? (1.0f / 65536.0f) : RayDir.y;
    RayDir.z = abs(RayDir.z) < (1.0f / 65536.0f) ? (1.0f / 65536.0f) : RayDir.z;
    const float3 InvRayDir = rcp(RayDir);

    const float3 MinIntersect = (NeighborMin - RayOrigin) * InvRayDir;
    const float3 MaxIntersect = (NeighborMax - RayOrigin) * InvRayDir;
    const float3 EnterIntersect = min(MinIntersect, MaxIntersect);
    return max(EnterIntersect.x, max(EnterIntersect.y, EnterIntersect.z));
}

float3 ClampHistory(float3 NeighborMin, float3 NeighborMax, float3 HistoryColor, float3 Filtered)
{
    if (all(HistoryColor >= NeighborMin) && all(HistoryColor <= NeighborMax))
        return HistoryColor;

    const float ClipBlend = saturate(HistoryClip(HistoryColor, Filtered, NeighborMin, NeighborMax));
    return lerp(HistoryColor, Filtered, ClipBlend);
}

float2 WeightedLerpFactors(float WeightA, float WeightB, float Blend)
{
    float BlendA = (1.0f - Blend) * WeightA;
    float BlendB = Blend * WeightB;
    const float RcpBlend = rcp(max(BlendA + BlendB, 0.0001f));
    return float2(BlendA * RcpBlend, BlendB * RcpBlend);
}

float2 GetVelocity(int2 screenST)
{
    uint2 texDim;
    DepthBuffer.GetDimensions(texDim.x, texDim.y);
    const int2 maxST = int2(texDim) - 1;

    float2 velocity = 0.0f.xx;
#if LONGEST_VELOCITY_VECTOR_SAMPLES
    const float2 offsets[8] = { float2(-1, -1), float2(-1, 0), float2(-1, 1), float2(0, 1), float2(1, 1), float2(1, 0), float2(1, -1), float2(0, -1) };
    float currentLengthSq = 0.0f;
    [unroll]
    for (uint i = 0; i < 8; ++i)
    {
        const int2 sampleST = clamp(screenST + int2(offsets[i]), int2(0, 0), maxST);
        const float2 neighborVelocity = VelocityBuffer.Load(int3(sampleST, 0)).xy;
        const float sampleLengthSq = dot(neighborVelocity, neighborVelocity);
        if (sampleLengthSq > currentLengthSq)
        {
            velocity = neighborVelocity;
            currentLengthSq = sampleLengthSq;
        }
    }
#else
    int2 closestOffset = int2(0, 0);
    float closestDepth = 1.0f;
    for (int y = -1; y <= 1; ++y)
    {
        for (int x = -1; x <= 1; ++x)
        {
            const int2 sampleST = clamp(screenST + int2(x, y), int2(0, 0), maxST);
            const float neighborhoodDepthSamp = DepthBuffer.Load(int3(sampleST, 0));
            if (neighborhoodDepthSamp < closestDepth)
            {
                closestDepth = neighborhoodDepthSamp;
                closestOffset = int2(x, y);
            }
        }
    }
    const int2 velST = clamp(screenST + closestOffset, int2(0, 0), maxST);
    velocity = VelocityBuffer.Load(int3(velST, 0)).xy;
#endif
    velocity *= float2(0.5f, -0.5f) * Resolution.xy;
    return velocity;
}

[numthreads(8, 8, 1)]
void TAA_Main(
    uint3 DTid : SV_DispatchThreadID,
    uint GI : SV_GroupIndex,
    uint3 GTid : SV_GroupThreadID,
    uint3 Gid : SV_GroupID)
{
    const float2 uv = GetUV(DTid.xy);
    if (uv.x > 1.0f || uv.y > 1.0f || uv.x < 0.0f || uv.y < 0.0f)
        return;

    if (FrameIndex <= 1)
    {
        OutTemporal[DTid.xy] = InColor[DTid.xy];
        return;
    }

    const float2 screenST = DTid.xy;

    const float2 velocity = GetVelocity(int2(screenST));
    const float velocityMagnitude = length(velocity);
    float velocityRejectPx = kVelocityRejectPixelsAtRef * (length(Resolution.xy) / max(length(kVelocityRefResolution), 1.0f));
    velocityRejectPx = max(velocityRejectPx, 24.0f);
    const float velocityNormalized = saturate(velocityMagnitude / max(velocityRejectPx, 1.0f));
    const float velocityConfidenceFactor = saturate(1.0f - velocityNormalized * velocityNormalized);

    const float2 historyScreenST = screenST - velocity;
    const float2 historyScreenUV = GetUV(historyScreenST);
    const float uvWeight = (all(historyScreenUV >= 0.0f.xx) && all(historyScreenUV <= 1.0f.xx)) ? 1.0f : 0.0f;
    const float motionConfidence = velocityConfidenceFactor * uvWeight;

    float3 prevColor = 0.0f;
    if (motionConfidence > 0.001f)
        prevColor = SampleHistory(historyScreenST);

    const float2 reconstructedSamplePos = screenST + 0.5f.xx;
    const float2 closestInputSampleST = floor(reconstructedSamplePos - CurrentJitterPixels.xy);
    const float2 closestInputSamplePos = closestInputSampleST + 0.5f.xx;
    const float2 dcenter = closestInputSamplePos + CurrentJitterPixels.xy - reconstructedSamplePos;

    float SampleWeights[9];
    float TotalWeight = 0.0f;
    for (int i = 0; i < 9; ++i)
    {
#if SPATIAL_WEIGHT_CATMULLROM
        SampleWeights[i] = Lanczos2(dcenter.x + (float)SampleOffsets[i].x) * Lanczos2(dcenter.y + (float)SampleOffsets[i].y);
#else
        const float ox = (float)SampleOffsets[i].x;
        const float oy = (float)SampleOffsets[i].y;
        SampleWeights[i] = exp(-2.29f * (ox * ox + oy * oy));
#endif
        TotalWeight += SampleWeights[i];
    }
    for (int w = 0; w < 9; ++w)
        SampleWeights[w] /= max(TotalWeight, 1e-6f);

    float3 neighborMin = float3(1e9f, 1e9f, 1e9f);
    float3 neighborMax = float3(-1e9f, -1e9f, -1e9f);
    float3 neighborhood[9];
    float3 NeighborsColor = 0.0f;
    float NeighborsFinalWeight = 0.0f;

    for (int y = -1; y <= 1; ++y)
    {
        for (int x = -1; x <= 1; ++x)
        {
            const int i = (y + 1) * 3 + x + 1;
            const float2 sampleST = closestInputSampleST + float2((float)x, (float)y);
            const float3 neighborhoodSamp = GetCurrentColour(sampleST);
            neighborhood[i] = neighborhoodSamp;
            neighborMin = min(neighborMin, neighborhoodSamp);
            neighborMax = max(neighborMax, neighborhoodSamp);
            const float sampleFinalWeight = SampleWeights[i];
            NeighborsColor += sampleFinalWeight * neighborhoodSamp;
            NeighborsFinalWeight += sampleFinalWeight;
        }
    }

    float3 FilteredColor = NeighborsColor * rcp(max(NeighborsFinalWeight, 0.0001f));
    FilteredColor = clamp(FilteredColor, neighborMin, neighborMax);

    const float3 box5Min = min(neighborhood[4], min(min(neighborhood[1], neighborhood[3]), min(neighborhood[5], neighborhood[7])));
    const float3 box5Max = max(neighborhood[4], max(max(neighborhood[1], neighborhood[3]), max(neighborhood[5], neighborhood[7])));
    const float3 aabbMin = lerp(box5Min, neighborMin, 0.5f);
    const float3 aabbMax = lerp(box5Max, neighborMax, 0.5f);

    const float3 m1 = neighborhood[4] + neighborhood[1] + neighborhood[3] + neighborhood[5] + neighborhood[7];
    const float3 m2 = neighborhood[4] * neighborhood[4] +
        neighborhood[1] * neighborhood[1] +
        neighborhood[3] * neighborhood[3] +
        neighborhood[5] * neighborhood[5] +
        neighborhood[7] * neighborhood[7];
    const float3 mu = m1 * 0.2f;
    const float3 sigma = sqrt(abs(m2 * 0.2f - mu * mu));

    float3 neighborClipMin = max(aabbMin, mu - VarianceGamma * sigma);
    float3 neighborClipMax = min(aabbMax, mu + VarianceGamma * sigma);

    if (motionConfidence > 0.001f)
        prevColor = ClampHistory(neighborClipMin, neighborClipMax, prevColor, FilteredColor);
    else
        prevColor = FilteredColor;

    float blendFinal = Feedback * motionConfidence;
    const float motionReject = 1.0f - motionConfidence;
    blendFinal = lerp(blendFinal, 0.25f, motionReject);

    const float3 centerRgb = YCoCgToRGB(neighborhood[4]);
    const float centerLin = Luminance(TaaToneCurveInv(centerRgb));
    const float react = saturate((centerLin - ReactiveLumaThreshold) * ReactiveLumaScale);
    blendFinal = lerp(blendFinal, max(blendFinal - ReactiveBlendSub, ReactiveBlendMin), react);
    blendFinal = saturate(blendFinal);

    const float filterWeight = HdrWeight4(FilteredColor, Exposure);
    const float historyWeight = HdrWeight4(prevColor, Exposure);
    const float2 weights = WeightedLerpFactors(historyWeight, filterWeight, blendFinal);
    float3 color = weights.x * prevColor + weights.y * FilteredColor;

    color = YCoCgToRGB(color);
    color = TaaToneCurveInv(color);
    OutTemporal[DTid.xy] = float4(color, 1.0f);
}
