// Single-tile directional shadow sampling for deferred / fur forward paths.
// Include after ShadowPCSS.hlsl and cbuffer cbDirectionalShadow (register b7) defining row_major matrix DirectionalShadowViewProj.

#ifndef MINIENGINE_DIRECTIONAL_SHADOW_HLSL
#define MINIENGINE_DIRECTIONAL_SHADOW_HLSL

float DirectionalShadowVisibility(float3 worldPos, float3 normal)
{
	float outVis = 1.0;
	float4 clip = mul(float4(worldPos, 1.0), DirectionalShadowViewProj);
	float w = clip.w;
	if (abs(w) >= 1e-6)
	{
		float3 proj = clip.xyz / w;
		if (proj.z > 0.0 && proj.z < 1.0)
		{
			float2 uvTile = proj.xy * float2(0.5, -0.5) + float2(0.5, 0.5);
			if (all(uvTile >= float2(0.0, 0.0)) && all(uvTile <= float2(1.0, 1.0)))
				outVis = clamp(ComputeShadowPCSSAtlasTile(uvTile, 0.0, 1.0, proj.z, normal, worldPos), 0.0, 1.0);
		}
	}
	return outVis;
}

/** [0,1] sun visibility for coupling skylight IBL into directional shadow (1 when shadow off / invalid). */
float PrimaryDirectionalShadowVisForIBL(float3 worldPos, float3 normal)
{
	float vis = 1.0;
	if (IsEnableShadow())
		vis = clamp(DirectionalShadowVisibility(worldPos, normal), 0.0, 1.0);
	return vis;
}

#endif // MINIENGINE_DIRECTIONAL_SHADOW_HLSL
