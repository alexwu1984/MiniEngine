// Fullscreen sky: cubemap directions must come from per-pixel unproject — interpolating unit-cube
// corner positions gives wrong rays (face seams / wedge-shaped gray regions after mips + trilinear).

#pragma pack_matrix(row_major)

TextureCube HdrCubeMap : register(t0);
SamplerState TrilinearFliterClamp : register(s0);

cbuffer CBSkyLightRenderPass : register(b0)
{
    matrix InvViewProj;
};

struct VertexOut
{
    float4 Pos : SV_POSITION;
    float4 WorldH : TEXCOORD0;
};

VertexOut VS_SkyFullscreen(uint VertID : SV_VertexID)
{
    // One oversized triangle that covers the entire [-1,1]^2 NDC square. A plain (-1,1)-(1,1)-(-1,-1)
    // triangle misses the bottom-right wedge (visible as a diagonal gray/black band — uncleared scene color).
    static const float2 kPositions[3] =
    {
        float2(-1.f, -1.f),
        float2(-1.f, 3.f),
        float2(3.f, -1.f),
    };
    float2 ndcXY = kPositions[VertID % 3];

    // Far clip point matching previous cube pass (z = w → ndc z = 1 after divide).
    float4 clipFar = float4(ndcXY, 1.f, 1.f);

    VertexOut Out;
    Out.Pos = clipFar;
    Out.WorldH = mul(clipFar, InvViewProj);
    return Out;
}

float4 PS(VertexOut In) : SV_Target
{
    float3 dir = normalize(In.WorldH.xyz / In.WorldH.w);
    float3 envColor = HdrCubeMap.Sample(TrilinearFliterClamp, dir).rgb;
    return float4(envColor, 1.f);
}
