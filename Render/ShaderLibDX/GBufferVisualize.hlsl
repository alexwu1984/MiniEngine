// Fullscreen GBuffer debug view (UE Buffer Visualization). Replaces SceneColor before post-process.
#include "ShaderUtils.hlsl"
#include "DeferredShadingCommon.hlsl"

#define GBUFFER_VIS_NONE 0
#define GBUFFER_VIS_BASECOLOR 1
#define GBUFFER_VIS_NORMAL 2
#define GBUFFER_VIS_METALLIC 3
#define GBUFFER_VIS_ROUGHNESS 4
#define GBUFFER_VIS_AO 5
#define GBUFFER_VIS_EMISSIVE 6
#define GBUFFER_VIS_DEPTH 7
#define GBUFFER_VIS_MATERIALAUX 8
#define GBUFFER_VIS_LIT_SCENECOLOR 9

cbuffer cbGBufferVisualize : register(b0)
{
	int Mode;
	float CameraNearZ;
	float CameraFarZ;
	float Pad0;
};

Texture2D BaseColorTex : register(t0);
Texture2D NormalTex : register(t1);
Texture2D EmissiveTex : register(t2);
Texture2D MRTex : register(t3);
Texture2D DepthTex : register(t4);
Texture2D MaterialAuxTex : register(t5);
Texture2D LitSceneColorTex : register(t6);
SamplerState LinearSampler : register(s0);

struct PSInput
{
	float2 Tex : TEXCOORD;
	float4 Pos : SV_Position;
};

PSInput VS_ScreenQuad(uint VertID : SV_VertexID)
{
	PSInput Out;
	float2 Tex = float2(uint2(VertID, VertID << 1) & 2);
	Out.Tex = Tex;
	Out.Pos = float4(lerp(float2(-1, 1), float2(1, -1), Tex), 0, 1);
	return Out;
}

float LinearizeDepthHw(float depthHw)
{
	const float n = max(CameraNearZ, 1e-4);
	const float f = max(CameraFarZ, n + 1e-3);
	const float denom = max(f - depthHw * (f - n), 1e-6);
	return (n * f) / denom;
}

float3 HueFromId(uint id)
{
	const float h = (float)(id % 12u) / 12.0;
	const float s = 0.85;
	const float v = 0.95;
	const float c = v * s;
	const float x = c * (1.0 - abs(fmod(h * 6.0, 2.0) - 1.0));
	float3 rgb = float3(0, 0, 0);
	const float m = v - c;
	if (h < 1.0 / 6.0) rgb = float3(c, x, 0);
	else if (h < 2.0 / 6.0) rgb = float3(x, c, 0);
	else if (h < 3.0 / 6.0) rgb = float3(0, c, x);
	else if (h < 4.0 / 6.0) rgb = float3(0, x, c);
	else if (h < 5.0 / 6.0) rgb = float3(x, 0, c);
	else rgb = float3(c, 0, x);
	return rgb + m;
}

float4 PS_GBufferVisualize(PSInput Input) : SV_Target0
{
	const float2 uv = Input.Tex;
	float depth = DepthTex.Sample(LinearSampler, uv).r;
	if (depth >= 0.99999)
		return float4(0.02, 0.02, 0.03, 1.0);

	float3 outRgb = float3(0, 0, 0);

	if (Mode == GBUFFER_VIS_BASECOLOR)
	{
		float4 b = BaseColorTex.Sample(LinearSampler, uv);
		outRgb = b.rgb;
	}
	else if (Mode == GBUFFER_VIS_NORMAL)
	{
		float3 n = NormalTex.Sample(LinearSampler, uv).xyz * 2.0 - 1.0;
		outRgb = n * 0.5 + 0.5;
	}
	else if (Mode == GBUFFER_VIS_METALLIC)
	{
		float m = MRTex.Sample(LinearSampler, uv).r;
		outRgb = float3(m, m, m);
	}
	else if (Mode == GBUFFER_VIS_ROUGHNESS)
	{
		float r = MRTex.Sample(LinearSampler, uv).b;
		outRgb = float3(r, r, r);
	}
	else if (Mode == GBUFFER_VIS_AO)
	{
		float ao = MRTex.Sample(LinearSampler, uv).g;
		outRgb = float3(ao, ao, ao);
	}
	else if (Mode == GBUFFER_VIS_EMISSIVE)
	{
		outRgb = EmissiveTex.Sample(LinearSampler, uv).rgb;
	}
	else if (Mode == GBUFFER_VIS_DEPTH)
	{
		const float viewZ = LinearizeDepthHw(depth);
		const float n = max(CameraNearZ, 1e-4);
		const float f = max(CameraFarZ, n + 1e-3);
		const float t = saturate((viewZ - n) / (f - n));
		outRgb = float3(t, t, t);
	}
	else if (Mode == GBUFFER_VIS_MATERIALAUX)
	{
		float4 aux = MaterialAuxTex.Sample(LinearSampler, uv);
		const uint smid = DecodeShadingModelId(aux.r);
		outRgb = HueFromId(smid);
	}
	else if (Mode == GBUFFER_VIS_LIT_SCENECOLOR)
	{
		outRgb = LitSceneColorTex.Sample(LinearSampler, uv).rgb;
	}
	else
	{
		outRgb = BaseColorTex.Sample(LinearSampler, uv).rgb;
	}

	return float4(outRgb, 1.0);
}
