#include "GLTFPbrPass-VS.hlsl"
#include "GLTFPbrPass-IO.hlsl"


Texture2D AlbedoMap : register(t0);
SamplerState SampleLinear : register(s0);

struct PS_OUTPUT_SCENE
{
	float4 Color : SV_Target0;
};

PS_OUTPUT_SCENE MainPS(VS_OUTPUT_SCENE Input) : SV_Target
{
	PS_OUTPUT_SCENE Output;
    Output.Color = AlbedoMap.Sample(SampleLinear, Input.UV1);
    return Output;
}
