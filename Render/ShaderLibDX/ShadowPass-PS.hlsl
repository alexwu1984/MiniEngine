// Do not include EnvironmentSkyIBL/EnvironmentShaders: they declare TextureCube at register(t0) for unrelated passes.
// This PS only declares 2D t0 when SHADOW_ALPHA_CLIP is enabled (blend + base color map).
#include "GLTFPbrPass-IO.hlsl"

#ifdef SHADOW_ALPHA_CLIP
cbuffer cbPerMaterial : register(b6)
{
	float Metallic;
	float AlphaCutoff;
	uint AlphaMask;
	uint Padding;
}
Texture2D AlbedoMap : register(t0);
SamplerState AlbedoSampler : register(s0);
#endif

float4 MainPS(VS_OUTPUT_SCENE Input) : SV_Target
{
#ifdef SHADOW_ALPHA_CLIP
	float a = AlbedoMap.Sample(AlbedoSampler, Input.UV0).a;
	clip(a - AlphaCutoff);
#endif
	// R32 shadow map: linear clip depth [0,1] (must match deferred mul(worldPos, LightViewProj).z / w).
	float z = Input.svPosition.z / max(Input.svPosition.w, 1e-6);
	return float4(z, 0.0, 0.0, 1.0);
}