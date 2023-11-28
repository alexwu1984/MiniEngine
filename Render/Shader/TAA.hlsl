#define RADIUS      1
#define GROUP_SIZE  8
#define TILE_DIM    (2 * RADIUS + GROUP_SIZE)

Texture2D ColorBuffer : register(t0);
Texture2D DepthBuffer : register(t1);
Texture2D HistoryBuffer : register(t2);
Texture2D VelocityBuffer : register(t3);

RWTexture2D<float4> OutputBuffer : register(u0);
SamplerState ColorSampler : register(s0);
SamplerState DepthSampler : register(s1);
SamplerState HistorySampler : register(s2);
SamplerState VelocitySampler : register(s3);

groupshared float3 Tile[TILE_DIM * TILE_DIM];

float2 GetClosestVelocity(in float2 uv, in float2 texelSize, out bool isSkyPixel)
{
    float2 velocity;
    float closestDepth = 9.9f;
    for (int y = -1; y <= 1; ++y)
        for (int x = -1; x <= 1; ++x)
        {
            const float2 st = uv + float2(x, y) * texelSize;
            const float depth = DepthBuffer.SampleLevel(DepthSampler, st, 0.0f).x;
            if (depth < closestDepth)
            {
                velocity = VelocityBuffer.SampleLevel(VelocitySampler, st, 0.0f).xy;
                closestDepth = depth;
            }
        }
    isSkyPixel = (closestDepth == 1.0f);
    return velocity * float2(0.5f, -0.5f); // from ndc to uv
}

float3 Reinhard(in float3 hdr)
{
    return hdr / (hdr + 1.0f);
}

float3 Tap(in float2 pos)
{
    return Tile[int(pos.x) + TILE_DIM * int(pos.y)];
}

float3 SampleHistoryCatmullRom(in float2 uv, in float2 texelSize)
{
    // Source: https://gist.github.com/TheRealMJP/c83b8c0f46b63f3a88a5986f4fa982b1
    // License: https://gist.github.com/TheRealMJP/bc503b0b87b643d3505d41eab8b332ae

    // We're going to sample a a 4x4 grid of texels surrounding the target UV coordinate. We'll do this by rounding
    // down the sample location to get the exact center of our "starting" texel. The starting texel will be at
    // location [1, 1] in the grid, where [0, 0] is the top left corner.
    float2 samplePos = uv / texelSize;
    float2 texPos1 = floor(samplePos - 0.5f) + 0.5f;

    // Compute the fractional offset from our starting texel to our original sample location, which we'll
    // feed into the Catmull-Rom spline function to get our filter weights.
    float2 f = samplePos - texPos1;

    // Compute the Catmull-Rom weights using the fractional offset that we calculated earlier.
    // These equations are pre-expanded based on our knowledge of where the texels will be located,
    // which lets us avoid having to evaluate a piece-wise function.
    float2 w0 = f * (-0.5f + f * (1.0f - 0.5f * f));
    float2 w1 = 1.0f + f * f * (-2.5f + 1.5f * f);
    float2 w2 = f * (0.5f + f * (2.0f - 1.5f * f));
    float2 w3 = f * f * (-0.5f + 0.5f * f);

    // Work out weighting factors and sampling offsets that will let us use bilinear filtering to
    // simultaneously evaluate the middle 2 samples from the 4x4 grid.
    float2 w12 = w1 + w2;
    float2 offset12 = w2 / (w1 + w2);

    // Compute the final UV coordinates we'll use for sampling the texture
    float2 texPos0 = texPos1 - 1.0f;
    float2 texPos3 = texPos1 + 2.0f;
    float2 texPos12 = texPos1 + offset12;

    texPos0 *= texelSize;
    texPos3 *= texelSize;
    texPos12 *= texelSize;

    float3 result = float3(0.0f, 0.0f, 0.0f);

    result += HistoryBuffer.SampleLevel(HistorySampler, float2(texPos0.x, texPos0.y), 0.0f).xyz * w0.x * w0.y;
    result += HistoryBuffer.SampleLevel(HistorySampler, float2(texPos12.x, texPos0.y), 0.0f).xyz * w12.x * w0.y;
    result += HistoryBuffer.SampleLevel(HistorySampler, float2(texPos3.x, texPos0.y), 0.0f).xyz * w3.x * w0.y;

    result += HistoryBuffer.SampleLevel(HistorySampler, float2(texPos0.x, texPos12.y), 0.0f).xyz * w0.x * w12.y;
    result += HistoryBuffer.SampleLevel(HistorySampler, float2(texPos12.x, texPos12.y), 0.0f).xyz * w12.x * w12.y;
    result += HistoryBuffer.SampleLevel(HistorySampler, float2(texPos3.x, texPos12.y), 0.0f).xyz * w3.x * w12.y;

    result += HistoryBuffer.SampleLevel(HistorySampler, float2(texPos0.x, texPos3.y), 0.0f).xyz * w0.x * w3.y;
    result += HistoryBuffer.SampleLevel(HistorySampler, float2(texPos12.x, texPos3.y), 0.0f).xyz * w12.x * w3.y;
    result += HistoryBuffer.SampleLevel(HistorySampler, float2(texPos3.x, texPos3.y), 0.0f).xyz * w3.x * w3.y;

    return max(result, 0.0f);
}

[numthreads(GROUP_SIZE, GROUP_SIZE, 1)]
void main(uint3 globalID : SV_DispatchThreadID, uint3 localID : SV_GroupThreadID, uint localIndex : SV_GroupIndex, uint3 groupID : SV_GroupID)
{
    int3 dims;
    bool isSkyPixel;

    // Populate private memory
    ColorBuffer.GetDimensions(0, dims.x, dims.y, dims.z);
    const float2 texelSize = 1.0f / float2(dims.xy);
    const float2 uv = (globalID.xy + 0.5f) * texelSize;
    const float2 tilePos = localID.xy + RADIUS + 0.5f;

    // Populate local memory
    if (localIndex < TILE_DIM * TILE_DIM / 4)
    {
        const int2 anchor = groupID.xy * GROUP_SIZE - RADIUS;

        const int2 coord1 = anchor + int2(localIndex % TILE_DIM, localIndex / TILE_DIM);
        const int2 coord2 = anchor + int2((localIndex + TILE_DIM * TILE_DIM / 4) % TILE_DIM, (localIndex + TILE_DIM * TILE_DIM / 4) / TILE_DIM);
        const int2 coord3 = anchor + int2((localIndex + TILE_DIM * TILE_DIM / 2) % TILE_DIM, (localIndex + TILE_DIM * TILE_DIM / 2) / TILE_DIM);
        const int2 coord4 = anchor + int2((localIndex + TILE_DIM * TILE_DIM * 3 / 4) % TILE_DIM, (localIndex + TILE_DIM * TILE_DIM * 3 / 4) / TILE_DIM);

        const float2 uv1 = (coord1 + 0.5f) * texelSize;
        const float2 uv2 = (coord2 + 0.5f) * texelSize;
        const float2 uv3 = (coord3 + 0.5f) * texelSize;
        const float2 uv4 = (coord4 + 0.5f) * texelSize;

        const float3 color0 = ColorBuffer.SampleLevel(ColorSampler, uv1, 0.0f).xyz;
        const float3 color1 = ColorBuffer.SampleLevel(ColorSampler, uv2, 0.0f).xyz;
        const float3 color2 = ColorBuffer.SampleLevel(ColorSampler, uv3, 0.0f).xyz;
        const float3 color3 = ColorBuffer.SampleLevel(ColorSampler, uv4, 0.0f).xyz;

        Tile[localIndex] = Reinhard(color0);
        Tile[localIndex + TILE_DIM * TILE_DIM / 4] = Reinhard(color1);
        Tile[localIndex + TILE_DIM * TILE_DIM / 2] = Reinhard(color2);
        Tile[localIndex + TILE_DIM * TILE_DIM * 3 / 4] = Reinhard(color3);
    }
    GroupMemoryBarrierWithGroupSync();

    // Iterate the neighboring samples
    if (any(int2(globalID.xy) >= dims.xy))
        return; // out of bounds

    float wsum = 0.0f;
    float3 vsum = float3(0.0f, 0.0f, 0.0f);
    float3 vsum2 = float3(0.0f, 0.0f, 0.0f);

    for (float y = -RADIUS; y <= RADIUS; ++y)
        for (float x = -RADIUS; x <= RADIUS; ++x)
        {
            const float3 neigh = Tap(tilePos + float2(x, y));
            const float w = exp(-3.0f * (x * x + y * y) / ((RADIUS + 1.0f) * (RADIUS + 1.0f)));
            vsum2 += neigh * neigh * w;
            vsum += neigh * w;
            wsum += w;
        }

    // Calculate mean and standard deviation
    const float3 ex = vsum / wsum;
    const float3 ex2 = vsum2 / wsum;
    const float3 dev = sqrt(max(ex2 - ex * ex, 0.0f));

    const float2 velocity = GetClosestVelocity(uv, texelSize, isSkyPixel);
    const float boxSize = lerp(0.5f, 2.5f, isSkyPixel ? 0.0f : smoothstep(0.02f, 0.0f, length(velocity)));

    // Reproject and clamp to bounding box
    const float3 nmin = ex - dev * boxSize;
    const float3 nmax = ex + dev * boxSize;

    const float3 history = SampleHistoryCatmullRom(uv - velocity, texelSize);
    const float3 clampedHistory = clamp(history, nmin, nmax);
    const float3 center = Tap(tilePos); // retrieve center value
    const float3 result = lerp(clampedHistory, center, 1.0f / 16.0f);

    // Write antialised sample to memory
    OutputBuffer[globalID.xy] = float4(result, 1.0f);
}

[numthreads(GROUP_SIZE, GROUP_SIZE, 1)]
void first(uint3 globalID : SV_DispatchThreadID, uint3 localID : SV_GroupThreadID, uint localIndex : SV_GroupIndex, uint3 groupID : SV_GroupID)
{
    int3 dims;
    bool isSkyPixel;

    // Populate private memory
    ColorBuffer.GetDimensions(0, dims.x, dims.y, dims.z);
    const float2 texelSize = 1.0f / float2(dims.xy);
    const float2 uv = (globalID.xy + 0.5f) * texelSize;
    const float2 tilePos = localID.xy + RADIUS + 0.5f;

    // Populate local memory
    if (localIndex < TILE_DIM * TILE_DIM / 4)
    {
        const int2 anchor = groupID.xy * GROUP_SIZE - RADIUS;

        const int2 coord1 = anchor + int2(localIndex % TILE_DIM, localIndex / TILE_DIM);
        const int2 coord2 = anchor + int2((localIndex + TILE_DIM * TILE_DIM / 4) % TILE_DIM, (localIndex + TILE_DIM * TILE_DIM / 4) / TILE_DIM);
        const int2 coord3 = anchor + int2((localIndex + TILE_DIM * TILE_DIM / 2) % TILE_DIM, (localIndex + TILE_DIM * TILE_DIM / 2) / TILE_DIM);
        const int2 coord4 = anchor + int2((localIndex + TILE_DIM * TILE_DIM * 3 / 4) % TILE_DIM, (localIndex + TILE_DIM * TILE_DIM * 3 / 4) / TILE_DIM);

        const float2 uv1 = (coord1 + 0.5f) * texelSize;
        const float2 uv2 = (coord2 + 0.5f) * texelSize;
        const float2 uv3 = (coord3 + 0.5f) * texelSize;
        const float2 uv4 = (coord4 + 0.5f) * texelSize;

        const float3 color0 = ColorBuffer.SampleLevel(ColorSampler, uv1, 0.0f).xyz;
        const float3 color1 = ColorBuffer.SampleLevel(ColorSampler, uv2, 0.0f).xyz;
        const float3 color2 = ColorBuffer.SampleLevel(ColorSampler, uv3, 0.0f).xyz;
        const float3 color3 = ColorBuffer.SampleLevel(ColorSampler, uv4, 0.0f).xyz;

        Tile[localIndex] = Reinhard(color0);
        Tile[localIndex + TILE_DIM * TILE_DIM / 4] = Reinhard(color1);
        Tile[localIndex + TILE_DIM * TILE_DIM / 2] = Reinhard(color2);
        Tile[localIndex + TILE_DIM * TILE_DIM * 3 / 4] = Reinhard(color3);
    }
    GroupMemoryBarrierWithGroupSync();
    const float3 center = Tap(tilePos); // retrieve center value
    OutputBuffer[globalID.xy] = float4(center, 1.0f);
}


//------------------------------------------------------- MACRO DEFINITION
#define SPATIAL_WEIGHT_CATMULLROM 1
#define LONGEST_VELOCITY_VECTOR_SAMPLES 0

//------------------------------------------------------- PARAMETERS
static const float Exposure = 10;
static const float BlendWeightLowerBound = 0.04f;
static const float BlendWeightUpperBound = 0.2f;
static const float MIN_VARIANCE_GAMMA = 0.75f; // under motion
static const float MAX_VARIANCE_GAMMA = 2.f; // no motion
static const float FRAME_VELOCITY_IN_PIXELS_DIFF = 128.0f; // valid for 1920x1080

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
    return rcp(Luma4(Color) * Exposure + 4.0);
}

float3 ToneMap(float3 color)
{
    // luma weight' tonemap
    return color / (1 + Luminance(color));
}

float3 UnToneMap(float3 color)
{
    // luma weight' untonemap
    return color / (1 - Luminance(color));
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

// Helper to convert ST coords to UV
float2 GetUV(float2 inST, float2 Resolution)
{
    return (inST + 0.5f.xx) * Resolution;
}

float3 GetCurrentColour(float2 screenST, float2 Resolution)
{
    float2 uv = GetUV(screenST, Resolution);
    float3 colour = ColorBuffer.SampleLevel(ColorSampler, uv, 0);
    //float3 colour = InColor[screenST];
    colour = ToneMap(colour);
    colour = RGBToYCoCg(colour);
    return colour;
}

float3 SampleHistory(float2 inHistoryST, float2 Resolution)
{
    float2 historyScreenUV = GetUV(inHistoryST, Resolution);
    // TODO: Sample the history using Catmull-Rom to reduce blur on motion.
    // https://www.shadertoy.com/view/4tyGDD

    float3 history = HistoryBuffer.SampleLevel(HistorySampler, historyScreenUV, 0).rgb;
    //float3 history = InTemporal[inHistoryST].rgb;
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
    float RcpBlend = rcp(BlendA + BlendB);
    BlendA *= RcpBlend;
    BlendB *= RcpBlend;
    return float2(BlendA, BlendB);
}

float2 GetVelocity(float2 uv, float2 Resolution)
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
    velocity *= float2(0.5, -0.5) * Resolution;
    return velocity;
}

[numthreads(8, 8, 1)]
void TAA_First(uint3 globalID : SV_DispatchThreadID, uint3 localID : SV_GroupThreadID, uint localIndex : SV_GroupIndex, uint3 groupID : SV_GroupID)
{
    int3 dims;

    // Populate private memory
    ColorBuffer.GetDimensions(0, dims.x, dims.y, dims.z);
    const float2 texelSize = 1.0f / float2(dims.xy);
    
    float2 uv = GetUV(globalID.xy, texelSize);
    if (uv.x > 1.0f || uv.y > 1.0f || uv.x < 0 || uv.y < 0)
    {
        return;
    }

    OutputBuffer[globalID.xy] = float4(ToneMap(ColorBuffer[globalID.xy].xyz), 1);
}

//------------------------------------------------------- ENTRY POINT
[numthreads(8, 8, 1)]
void TAA_Main(
    uint3 DTid : SV_DispatchThreadID,
    uint GI : SV_GroupIndex,
    uint3 GTid : SV_GroupThreadID,
    uint3 Gid : SV_GroupID)
{
    int3 dims;

    // Populate private memory
    ColorBuffer.GetDimensions(0, dims.x, dims.y, dims.z);
    const float2 texelSize = 1.0f / float2(dims.xy);
    
    float2 uv = GetUV(DTid.xy, texelSize);
    if (uv.x > 1.0f || uv.y > 1.0f || uv.x < 0 || uv.y < 0)
    {
        return;
    }

    // screenPos
    const float2 screenST = DTid.xy;


    float2 velocity = GetVelocity(screenST, float2(dims.xy));
    // calculate confidence factor based on the velocity of current pixel, everything moving faster than FRAME_VELOCITY_IN_PIXELS_DIFF frame-to-frame will be marked as no-history
    const float velocityConfidenceFactor = saturate(1.f - length(velocity) / FRAME_VELOCITY_IN_PIXELS_DIFF);

    const float2 historyScreenST = screenST - velocity;
    const float2 historyScreenUV = GetUV(historyScreenST, texelSize);
    const float uvWeight = (historyScreenUV >= float2(0.f, 0.f) && historyScreenUV <= float2(1.f, 1.f)) ? 1.0f : 0.f;
    const bool hasValidHistory = (velocityConfidenceFactor * uvWeight) > 0.f;

    if (hasValidHistory == false)
    {
        OutputBuffer[DTid.xy] = float4(ToneMap(ColorBuffer[DTid.xy].xyz), 1);
        return;
    }

    // current frame color
    float3 currColor = GetCurrentColour(screenST, texelSize);

    // sample history color
    float3 prevColor = SampleHistory(historyScreenST, texelSize);
    
    // SetupSampleWeight
    float SampleWeights[9];
    float TotalWeight = 0.0f;
    for (int i = 0; i < 9; i++)
    {
        float PixelOffsetX = SampleOffsets[i].x;
        float PixelOffsetY = SampleOffsets[i].y;

#if SPATIAL_WEIGHT_CATMULLROM
        SampleWeights[i] = CatmullRom(PixelOffsetX) * CatmullRom(PixelOffsetY);
        TotalWeight += SampleWeights[i];
#else
        // Normal distribution, Sigma = 0.47
        SampleWeights[i] = exp(-2.29f * (PixelOffsetX * PixelOffsetX + PixelOffsetY * PixelOffsetY));
        TotalWeight += SampleWeights[i];
#endif
    }
    for (int i = 0; i < 9; i++)
    {
        SampleWeights[i] /= TotalWeight;
    }

    // sample neighborhoods
    uint N = 9;
    float3 m1 = 0.0f;
    float3 m2 = 0.0f;
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
            float2 sampleST = screenST + sampleOffset;

            // sample
            float3 NeighborhoodSamp = GetCurrentColour(sampleST, texelSize);

            // cache
            neighborhood[i] = NeighborhoodSamp;

            // AAABB
            neighborMin = min(neighborMin, NeighborhoodSamp);
            neighborMax = max(neighborMax, NeighborhoodSamp);

            m1 += NeighborhoodSamp;
            m2 += NeighborhoodSamp * NeighborhoodSamp;

            float SampleSpatialWeight = SampleWeights[i];
            float SampleHdrWeight = HdrWeight4(NeighborhoodSamp, Exposure);

            // combine two weight
            float SampleFinalWeight = SampleSpatialWeight * SampleHdrWeight;
            
            NeighborsColor += SampleFinalWeight * NeighborhoodSamp;
            NeighborsFinalWeight += SampleFinalWeight;
        }
    }

    // compute filteredColor
    FilteredColor = NeighborsColor * rcp(max(NeighborsFinalWeight, 0.0001));

    // shappen 
    float3 highFreq = neighborhood[1] + neighborhood[3] + neighborhood[5] + neighborhood[7] - 4 * neighborhood[4];
    FilteredColor += highFreq * 0.1f;

    float LumaHistory = Luma4(prevColor);
    // simplest clip
    //prevColor = clamp(prevColor, neighborMin, neighborMax);

    // neighborhood clamping
    //prevColor = ClampHistory(neighborMin, neighborMax, prevColor, (neighborMin + neighborMax) / 2.0f);

    // variance clip
    float3 mu = m1 / N;
    float3 sigma = sqrt(abs(m2 / N - mu * mu));
    float VarianceClipGamma = lerp(MIN_VARIANCE_GAMMA, MAX_VARIANCE_GAMMA, velocityConfidenceFactor);
    neighborMin = mu - VarianceClipGamma * sigma;
    neighborMax = mu + VarianceClipGamma * sigma;
    prevColor = ClampHistory(neighborMin, neighborMax, prevColor, mu);

    // compute blend amount 
    float BlendFinal;
    {
        float LumaFiltered = Luma4(FilteredColor);
        BlendFinal = lerp(BlendWeightLowerBound, BlendWeightUpperBound, saturate(1 - velocityConfidenceFactor));
        //BlendFinal = max(BlendFinal, saturate(0.01 * LumaHistory / abs(LumaFiltered - LumaHistory)));
    }

    float FilterWeight = HdrWeight4(FilteredColor, Exposure);
    float HistoryWeight = HdrWeight4(prevColor, Exposure);

    float2 Weights = WeightedLerpFactors(HistoryWeight, FilterWeight, BlendFinal);
    float3 color = Weights.x * prevColor + Weights.y * FilteredColor;

    color = YCoCgToRGB(color);
   // color = UnToneMap(color);

    OutputBuffer[DTid.xy] = float4(color, 1);
}