// Fullscreen sky: cubemap directions must come from per-pixel unproject — interpolating unit-cube
// corner positions gives wrong rays (face seams / wedge-shaped gray regions after mips + trilinear).

#pragma pack_matrix(row_major)

TextureCube HdrCubeMap : register(t0);
SamplerState TrilinearFliterClamp : register(s0);

cbuffer CBSkyLightRenderPass : register(b0)
{
    matrix InvViewProj;
    float3 SunTowardSource;
    float SunBloomLinearHDR;
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

    // Bloom-oriented forward scatter (not in baked cubemap). Soft lobes + warm low-sun tint — avoid a flat white disc.
    if (SunBloomLinearHDR > 0.0)
    {
        float3 s = SunTowardSource;
        float sl = dot(s, s);
        if (sl > 1e-8)
        {
            s *= rsqrt(sl);
            float mu = saturate(dot(dir, s));

            float broad = pow(saturate((mu - 0.78) / 0.22), 2.1);
            float mid = pow(saturate((mu - 0.91) / 0.09), 2.8);
            float tight = pow(saturate((mu - 0.994) / 0.006), 4.5);
            float lobe = broad * 0.14 + mid * 0.22 + tight * 0.10;
            lobe = min(lobe, 0.55);

            float sunElev = saturate(s.y);
            float golden = pow(saturate(1.0 - sunElev / 0.52), 1.35);
            float3 day = float3(1.02, 0.97, 0.93);
            float3 dusk = float3(1.12, 0.84, 0.42);
            float3 tint = lerp(day, dusk, golden);

            envColor += tint * (SunBloomLinearHDR * 0.34 * lobe);
        }
    }

    return float4(envColor, 1.f);
}
