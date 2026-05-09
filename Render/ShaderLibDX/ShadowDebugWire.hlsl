// World-space debug lines (shadow frusta / point bounds) composited after tonemapping, same matrix row-vector convention as GLTF shaders.
#pragma pack_matrix(row_major)

cbuffer ShadowDebugWireCB : register(b0)
{
	matrix WorldToClip;
};

struct VertexIn
{
	float3 Pos : ATTRIBUTE0;
	float4 Color : ATTRIBUTE1;
};

struct VertexOut
{
	float4 Pos : SV_POSITION;
	float4 Color : COLOR0;
};

VertexOut VS(VertexIn In)
{
	VertexOut Out;
	Out.Pos = mul(float4(In.Pos, 1.0f), WorldToClip);
	Out.Color = In.Color;
	return Out;
}

float4 PS(VertexOut In) : SV_Target
{
	return float4(In.Color.rgb, 1.0f);
}
