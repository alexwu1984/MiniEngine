// Do not include EnvironmentSkyIBL/EnvironmentShaders: they declare TextureCube at register(t0), while shadow depth
// binds a 2D SRV at t0 → D3D12 GBV #940 (SRV dimension mismatch). PS only needs VS_OUTPUT_SCENE.
#include "GLTFPbrPass-IO.hlsl"

float4 MainPS(VS_OUTPUT_SCENE Input) : SV_Target
{
	// R32 shadow map: linear clip depth [0,1] (must match deferred mul(worldPos, LightViewProj).z / w).
	float z = Input.svPosition.z / max(Input.svPosition.w, 1e-6);
	return float4(z, 0.0, 0.0, 1.0);
}