// Do not include EnvironmentShaders.hlsl: it also binds register(t0) (Cube / 2D helpers), which confuses
// reflection and D3D12 GBV #940 (pixel t0 expects TEXTURECUBE vs bound TEXTURE2D) for this pass.
// Must match engine row-major matrices (previously came in via EnvironmentShaders → ShaderUtils).
#pragma pack_matrix(row_major)

TextureCube HdrCubeMap : register(t0);
SamplerState TrilinearFliterClamp : register(s0);

cbuffer CBMatrix : register(b0)
{
    matrix View;
    matrix Proj;
};

struct VertexIn
{
    float3 Pos: ATTRIBUTE0;
};

struct VertexOut
{
    float4 Pos : SV_POSITION;
    float3 LocalPos : TEXCOORD0;
};


VertexOut VS(VertexIn ina)
{
    VertexOut outa;
    outa.LocalPos = ina.Pos;
    outa.Pos = mul(float4(ina.Pos, 1.0f), View);
    outa.Pos = mul(outa.Pos, Proj);
    outa.Pos.z = outa.Pos.w;
    return outa;
}

float4 PS(VertexOut outa) : SV_Target
{
    float3 envColor = HdrCubeMap.Sample(TrilinearFliterClamp, normalize(outa.LocalPos)).rgb;
    envColor = envColor / (envColor + 1.0);
    return float4(envColor, 1.0f);
}