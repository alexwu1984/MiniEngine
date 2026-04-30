// Do not include EnvironmentSkyIBL/EnvironmentShaders: they declare TextureCube at register(t0), while shadow depth
// binds a 2D SRV at t0 → D3D12 GBV #940 (SRV dimension mismatch). PS only needs VS_OUTPUT_SCENE.
#include "GLTFPbrPass-IO.hlsl"

float4 MainPS(VS_OUTPUT_SCENE Input) : SV_Target
{
	float depth = Input.svPosition.z;
    return float4(depth, depth * depth, 1.0f, 1.0f);
}