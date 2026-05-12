// Do not include EnvironmentSkyIBL/EnvironmentShaders: they declare TextureCube at register(t0) for unrelated passes.
#include "GLTFPbrPass-IO.hlsl"
#include "PerFrameStruct.hlsl"

cbuffer cbPerMaterial : register(b6)
{
	MaterialPerFrame myMaterial;
}

Texture2D AlbedoMap : register(t0);
SamplerState AlbedoSampler : register(s0);

void MainPS(VS_OUTPUT_SCENE Input)
{
	if ((myMaterial.MaterialShaderFlags & kMatShaderFlag_ShadowAlphaClip) != 0)
	{
		float a = AlbedoMap.Sample(AlbedoSampler, Input.UV0).a;
		clip(a - myMaterial.AlphaCutoff);
	}
}
