#define RADIUS      1
#define GROUP_SIZE  16
#define TILE_DIM    (2 * RADIUS + GROUP_SIZE)

Texture2D ColorBuffer : register(t0);

RWTexture2D<float4> OutputBuffer : register(u0);
SamplerState ColorSampler : register(s0);

groupshared float3 Tile[TILE_DIM * TILE_DIM];

float3 Reinhard(in float3 hdr)
{
    return hdr / (hdr + 1.0f);
}

float3 Tap(in float2 pos)
{
    return Tile[int(pos.x) + TILE_DIM * int(pos.y)];
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