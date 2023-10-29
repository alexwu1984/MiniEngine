#include "EnvironmentShaders.hlsl"


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
    //envColor = pow(envColor, 1.0 / 2.2);
    return float4(ACESToneMapping(envColor), 1.0);
}