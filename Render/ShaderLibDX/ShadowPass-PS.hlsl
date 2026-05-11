// Do not include EnvironmentSkyIBL/EnvironmentShaders: they declare TextureCube at register(t0) for unrelated passes.
#include "GLTFPbrPass-IO.hlsl"
#include "PerFrameStruct.hlsl"

cbuffer cbPerMaterial : register(b6)
{
	MaterialPerFrame myMaterial;
}

Texture2D AlbedoMap : register(t0);
SamplerState AlbedoSampler : register(s0);

float4 MainPS(VS_OUTPUT_SCENE Input) : SV_Target
{
	if ((myMaterial.MaterialShaderFlags & kMatShaderFlag_ShadowAlphaClip) != 0)
	{
		float a = AlbedoMap.Sample(AlbedoSampler, Input.UV0).a;
		clip(a - myMaterial.AlphaCutoff);
	}
	float z = Input.svPosition.z / max(Input.svPosition.w, 1e-6);
	return float4(z, 0.0, 0.0, 1.0);
}
