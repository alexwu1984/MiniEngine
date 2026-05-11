#ifndef FurPassIO
#define FurPassIO

//--------------------------------------------------------------------------------------
// Fur forward VS → PS linkage only (do not use in PBR / shadow — avoids extra TEXCOORD).
//--------------------------------------------------------------------------------------
struct VS_OUTPUT_FUR
{
	float4 svPosition : SV_POSITION;
	float3 WorldPos : POSITION0;
	float4 LightPos : POSITION1;
	float3 Normal : NORMAL;
	float3 SH : NORMAL1;
	float3 Tangent : TANGENT;
	float3 Binormal : BINORMAL;
	float2 UV0 : TEXCOORD0;
	float2 UV1 : TEXCOORD1;
	float4 svCurrPosition : TEXCOORD2;
	float4 svPrevPosition : TEXCOORD3;
	nointerpolation float FurShellOffset : TEXCOORD6;
};

#endif
