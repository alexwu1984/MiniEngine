
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

//------------------------------------------------------- MACRO DEFINITION
#define SPATIAL_WEIGHT_CATMULLROM 1
#define LONGEST_VELOCITY_VECTOR_SAMPLES 0

//------------------------------------------------------- PARAMETERS
static const float Exposure = 10;
// Filament default feedback: current-frame contribution.
static const float Feedback = 0.08f;
static const float VarianceGamma = 1.0f;
static const float FRAME_VELOCITY_IN_PIXELS_DIFF = 128.0f;  // valid for 1920x1080

static const int2 SampleOffsets[9] =
{
    int2(-1, -1),
    int2(0, -1),
    int2(1, -1),
    int2(-1, 0),
    int2(0, 0),
    int2(1, 0),
    int2(-1, 1),
    int2(0, 1),
    int2(1, 1),
};

//------------------------------------------------------- HELP FUNCTIONS

float Luminance(in float3 color)
{
    return dot(color, float3(0.25f, 0.50f, 0.25f));
}

// Faster but less accurate luma computation. 
// Luma includes a scaling by 4.
float Luma4(float3 Color)
{
    return Color.r;
}

// Optimized HDR weighting function.
float HdrWeight4(float3 Color, float Exposure)
{
    // Ensure Luma4 is non-negative to prevent invalid weights
    float luma = max(Luma4(Color), 0.0);
    return rcp(luma * Exposure + 4.0);
}

float3 ToneMap(float3 color)
{
    // luma weight' tonemap
    return color / (1 + Luminance(color));
}

float3 UnToneMap(float3 color)
{
    // luma weight' untonemap
    // Prevent division by zero or near-zero (which causes black pixels/flickering)
    float luma = Luminance(color);
    float denom = 1 - luma;
    // Clamp denominator to prevent division by zero or very small values
    denom = max(denom, 0.001f);
    return color / denom;
}

float3 RGBToYCoCg(float3 RGB)
{
    float Y = dot(RGB, float3(1, 2, 1));
    float Co = dot(RGB, float3(2, 0, -2));
    float Cg = dot(RGB, float3(-1, 2, -1));

    float3 YCoCg = float3(Y, Co, Cg);
    return YCoCg;
}

float3 YCoCgToRGB(float3 YCoCg)
{
    float Y = YCoCg.x * 0.25;
    float Co = YCoCg.y * 0.25;
    float Cg = YCoCg.z * 0.25;

    float R = Y + Co - Cg;
    float G = Y + Cg;
    float B = Y - Co - Cg;

    float3 RGB = float3(R, G, B);
    return RGB;
}

static float CatmullRom(float x)
{
    float ax = abs(x);
    if (ax > 1.0f)
        return ((-0.5f * ax + 2.5f) * ax - 4.0f) * ax + 2.0f;
    else
        return (1.5f * ax - 2.5f) * ax * ax + 1.0f;
}

// 1-D Lanczos-2 filter, matching Filament's jitter-aware input reconstruction.
float Lanczos2(float x)
{
    float x2 = x * x;
    float wA = x2 - 4.0f;
    float wB = x2 * wA - wA;
    wA *= wA;
    wB *= wA;
    return wB * (1.0f / 64.0f);
}

// Helper to convert ST coords to UV
float2 GetUV(float2 inST)
{
    return (inST + 0.5f.xx) * Resolution.zw;
}

float3 GetCurrentColour(float2 screenST)
{
    float2 uv = GetUV(screenST);
    float3 colour = InColor.SampleLevel(MinMagLinearMipPointClamp, uv, 0).rgb;
    //float3 colour = InColor[screenST];
    colour = ToneMap(colour);
    colour = RGBToYCoCg(colour);
    return colour;
}

float3 SampleHistory(float2 inHistoryST)
{
    float2 historyScreenUV = GetUV(inHistoryST);

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

    float k0 = w12.x * w0.y;
    float k1 = w0.x * w12.y;
    float k2 = w12.x * w12.y;
    float k3 = w3.x * w12.y;
    float k4 = w12.x * w3.y;

    float3 s0 = InTemporal.SampleLevel(MinMagLinearMipPointClamp, float2(texPos12.x, texPos0.y), 0).rgb;
    float3 s1 = InTemporal.SampleLevel(MinMagLinearMipPointClamp, float2(texPos0.x, texPos12.y), 0).rgb;
    float3 s2 = InTemporal.SampleLevel(MinMagLinearMipPointClamp, float2(texPos12.x, texPos12.y), 0).rgb;
    float3 s3 = InTemporal.SampleLevel(MinMagLinearMipPointClamp, float2(texPos3.x, texPos12.y), 0).rgb;
    float3 s4 = InTemporal.SampleLevel(MinMagLinearMipPointClamp, float2(texPos12.x, texPos3.y), 0).rgb;

    float3 history = (k0 * s0 + k1 * s1 + k2 * s2 + k3 * s3 + k4 * s4) * rcp(max(k0 + k1 + k2 + k3 + k4, 0.0001f));
    float3 boxMin = min(min(min(s0, s1), min(s2, s3)), s4);
    float3 boxMax = max(max(max(s0, s1), max(s2, s3)), s4);
    history = clamp(history, boxMin, boxMax);

    history = ToneMap(history);
    history = RGBToYCoCg(history);
    return history;
}

float HistoryClip(float3 History, float3 Filtered, float3 NeighborMin, float3 NeighborMax)
{
    float3 BoxMin = NeighborMin;
    float3 BoxMax = NeighborMax;

    float3 RayOrigin = History;
    float3 RayDir = Filtered - History;
    RayDir.x = abs(RayDir.x) < (1.0 / 65536.0) ? (1.0 / 65536.0) : RayDir.x;
    RayDir.y = abs(RayDir.y) < (1.0 / 65536.0) ? (1.0 / 65536.0) : RayDir.y;
    RayDir.z = abs(RayDir.z) < (1.0 / 65536.0) ? (1.0 / 65536.0) : RayDir.z;
    float3 InvRayDir = rcp(RayDir);

    float3 MinIntersect = (BoxMin - RayOrigin) * InvRayDir;
    float3 MaxIntersect = (BoxMax - RayOrigin) * InvRayDir;
    float3 EnterIntersect = min(MinIntersect, MaxIntersect);
    return max(EnterIntersect.x, max(EnterIntersect.y, EnterIntersect.z));
}

float3 ClampHistory(float3 NeighborMin, float3 NeighborMax, float3 HistoryColor, float3 Filtered)
{
    float3 TargetColor = Filtered;

    float ClipBlend = HistoryClip(HistoryColor, TargetColor, NeighborMin, NeighborMax);

    ClipBlend = saturate(ClipBlend);

    HistoryColor = lerp(HistoryColor, TargetColor, ClipBlend);

    return HistoryColor;
}

float2 WeightedLerpFactors(float WeightA, float WeightB, float Blend)
{
    float BlendA = (1.0 - Blend) * WeightA;
    float BlendB = Blend * WeightB;
    float TotalWeight = BlendA + BlendB;
    // Prevent division by zero - use epsilon to avoid black pixels
    float RcpBlend = rcp(max(TotalWeight, 0.0001f));
    BlendA *= RcpBlend;
    BlendB *= RcpBlend;
    return float2(BlendA, BlendB);
}

float2 GetVelocity(float2 uv)
{
    float2 velocity = 0;
#if LONGEST_VELOCITY_VECTOR_SAMPLES
    const float2 offsets[8] = { float2(-1, -1), float2(-1, 0), float2(-1, 1), float2(0, 1), float2(1, 1), float2(1, 0), float2(1, -1), float2(0, -1) };
    const uint numberOfSamples = 8;

    float currentLengthSq = dot(velocity.xy, velocity.xy);
    [unroll]
    for (uint i = 0; i < numberOfSamples; ++i)
    {
        const float2 neighbor_velocity = VelocityBuffer[uv + offsets[i]].xy;
        const float sampleLengthSq = dot(neighbor_velocity.xy, neighbor_velocity.xy);
        if (sampleLengthSq > currentLengthSq)
        {
            velocity = neighbor_velocity;
            currentLengthSq = sampleLengthSq;
        }
    }
#else
    // 3x3 closest depth
    float2 closestOffset = float2(0.0f, 0.0f);
    float closestDepth = 1.0f;
    for (int y = -1; y <= 1; ++y)
    {
        for (int x = -1; x <= 1; ++x)
        {
            float2 sampleOffset = float2(x, y);
            float2 sampleUV = uv + sampleOffset;

            float NeighborhoodDepthSamp = DepthBuffer[sampleUV];

            if (NeighborhoodDepthSamp < closestDepth)
            {
                closestDepth = NeighborhoodDepthSamp;
                closestOffset = sampleOffset;
            }
        }
    }
    velocity = VelocityBuffer[uv + closestOffset].xy;

#endif 
    velocity *= float2(0.5, -0.5) * Resolution.xy;
    return velocity;
}

//------------------------------------------------------- ENTRY POINT
[numthreads(8, 8, 1)]
void TAA_Main(
    uint3 DTid : SV_DispatchThreadID,
    uint GI : SV_GroupIndex,
    uint3 GTid : SV_GroupThreadID,
    uint3 Gid : SV_GroupID)
{
    float2 uv = GetUV(DTid.xy);
    if (uv.x > 1.0f || uv.y > 1.0f || uv.x <  0 || uv.y < 0)
    {
        return;
    }

    // first frame use currentColor
    if (FrameIndex <= 1.0f)
    {
        OutTemporal[DTid.xy] = InColor[DTid.xy];
        return;
    }

    // screenPos
    const float2 screenST = DTid.xy;


    float2 velocity = GetVelocity(screenST);
    float velocityMagnitude = length(velocity);
    // calculate confidence factor based on the velocity of current pixel, everything moving faster than FRAME_VELOCITY_IN_PIXELS_DIFF frame-to-frame will be marked as no-history
    // Use quadratic falloff for faster rejection of history during high-speed motion
    const float velocityNormalized = saturate(velocityMagnitude / FRAME_VELOCITY_IN_PIXELS_DIFF);
    const float velocityConfidenceFactor = saturate(1.f - velocityNormalized * velocityNormalized);

    const float2 historyScreenST = screenST - velocity;
    const float2 historyScreenUV = GetUV(historyScreenST);
    const float uvWeight = (all(historyScreenUV >= float2(0.f, 0.f)) && all(historyScreenUV <= float2(1.f, 1.f))) ? 1.0f : 0.f;
    const bool hasValidHistory = (velocityConfidenceFactor * uvWeight) > 0.f;

    if (hasValidHistory == false)
    {
        OutTemporal[DTid.xy] = InColor[DTid.xy];
        return;
    }

    // current frame color
    float3 currColor = GetCurrentColour(screenST);

    // sample history color
    float3 prevColor = SampleHistory(historyScreenST);
    
    // SetupSampleWeight
    float2 reconstructedSamplePos = screenST + 0.5f.xx;
    float2 closestInputSampleST = floor(reconstructedSamplePos - CurrentJitterPixels.xy);
    float2 closestInputSamplePos = closestInputSampleST + 0.5f.xx;
    float2 dcenter = closestInputSamplePos + CurrentJitterPixels.xy - reconstructedSamplePos;

    float SampleWeights[9];
    float TotalWeight = 0.0f;
    for (int i = 0; i < 9; i++)
    {
        float PixelOffsetX = SampleOffsets[i].x;
        float PixelOffsetY = SampleOffsets[i].y;

#if SPATIAL_WEIGHT_CATMULLROM
        SampleWeights[i] = Lanczos2(dcenter.x + PixelOffsetX) * Lanczos2(dcenter.y + PixelOffsetY);
        TotalWeight += SampleWeights[i];
#else
        // Normal distribution, Sigma = 0.47
        SampleWeights[i] = exp(-2.29f * (PixelOffsetX * PixelOffsetX + PixelOffsetY * PixelOffsetY));
        TotalWeight += SampleWeights[i];
#endif
    }
    for (int WeightIndex = 0; WeightIndex < 9; WeightIndex++)
    {
        SampleWeights[WeightIndex] /= TotalWeight;
    }

    // sample neighborhoods
    float3 neighborMin = float3(9999999.0f, 9999999.0f, 9999999.0f);
    float3 neighborMax = float3(-99999999.0f, -99999999.0f, -99999999.0f);

    // used for FilterColor
    float3 neighborhood[9];
    float NeighborsFinalWeight = 0;
    float3 NeighborsColor = 0;
    float3 FilteredColor = 0;
    
    for (int y = -1; y <= 1; ++y)
    {
        for (int x = -1; x <= 1; ++x)
        {
            // convert to [0,8]
            int i = (y + 1) * 3 + x + 1;

            // offset
            float2 sampleOffset = float2(x, y);
            float2 sampleST = closestInputSampleST + sampleOffset;

            // sample
            float3 NeighborhoodSamp = GetCurrentColour(sampleST);

            // cache
            neighborhood[i] = NeighborhoodSamp;

            // AAABB
            neighborMin = min(neighborMin, NeighborhoodSamp);
            neighborMax = max(neighborMax, NeighborhoodSamp);

            float SampleFinalWeight = SampleWeights[i];
            
            NeighborsColor += SampleFinalWeight * NeighborhoodSamp;
            NeighborsFinalWeight += SampleFinalWeight;
        }
    }

    // compute filteredColor
    FilteredColor = NeighborsColor * rcp(max(NeighborsFinalWeight, 0.0001));
    FilteredColor = clamp(FilteredColor, neighborMin, neighborMax);

    float3 box5Min = min(neighborhood[4], min(min(neighborhood[1], neighborhood[3]), min(neighborhood[5], neighborhood[7])));
    float3 box5Max = max(neighborhood[4], max(max(neighborhood[1], neighborhood[3]), max(neighborhood[5], neighborhood[7])));
    float3 aabbMin = lerp(box5Min, neighborMin, 0.5f);
    float3 aabbMax = lerp(box5Max, neighborMax, 0.5f);

    // variance clip
    float3 m1 = neighborhood[4] + neighborhood[1] + neighborhood[3] + neighborhood[5] + neighborhood[7];
    float3 m2 = neighborhood[4] * neighborhood[4] +
        neighborhood[1] * neighborhood[1] +
        neighborhood[3] * neighborhood[3] +
        neighborhood[5] * neighborhood[5] +
        neighborhood[7] * neighborhood[7];
    float3 mu = m1 * 0.2f;
    float3 sigma = sqrt(abs(m2 * 0.2f - mu * mu));
    neighborMin = max(aabbMin, mu - VarianceGamma * sigma);
    neighborMax = min(aabbMax, mu + VarianceGamma * sigma);
    prevColor = ClampHistory(neighborMin, neighborMax, prevColor, mu);

    // compute blend amount 
    float BlendFinal;
    {
        BlendFinal = Feedback;
        // Ensure blend weight is within valid range
        BlendFinal = saturate(BlendFinal);
    }

    float FilterWeight = HdrWeight4(FilteredColor, Exposure);
    float HistoryWeight = HdrWeight4(prevColor, Exposure);

    float2 Weights = WeightedLerpFactors(HistoryWeight, FilterWeight, BlendFinal);
    float3 color = Weights.x * prevColor + Weights.y * FilteredColor;

    color = YCoCgToRGB(color);
    color = UnToneMap(color);
    
    OutTemporal[DTid.xy] = float4(color, 1);
}
