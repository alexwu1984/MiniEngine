#include "EnvironmentShaders.hlsl"
#include "GLTFPbrPass-VS.hlsl"

float4 MainPS(VS_OUTPUT_SCENE Input) : SV_Target
{
	float depth = Input.svPosition.z/Input.svPosition.w;
    return float4(depth, depth * depth, 1.0f, 1.0f);
}