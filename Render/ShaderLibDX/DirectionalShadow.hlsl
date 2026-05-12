// Directional shadow visibility (single tile or vertical CSM atlas). Include after ShadowPCSS.hlsl + DirectionalShadowCB.hlsl + PerFrameStruct.
#ifndef MINIENGINE_DIRECTIONAL_SHADOW_HLSL
#define MINIENGINE_DIRECTIONAL_SHADOW_HLSL

float DirectionalShadowVisibility(float3 worldPos, float3 normal)
{
	float outVis = 1.0;
	const float3 camPos = myPerFrame.CameraPos.xyz;
	if (DirectionalCSMEnabled == 0)
	{
		float4 clip = mul(float4(worldPos, 1.0), CascadeViewProj[0]);
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

	const float3 fdir = CameraForwardInvCount.xyz;
	const float ze = dot(worldPos - camPos, fdir);
	int idx = 0;
	if (CascadeCount >= 2 && ze >= CascadeSplits.x)
		idx = 1;
	if (CascadeCount >= 3 && ze >= CascadeSplits.y)
		idx = 2;
	idx = clamp(idx, 0, CascadeCount - 1);

	float4 clip = mul(float4(worldPos, 1.0), CascadeViewProj[idx]);
	float w = clip.w;
	if (abs(w) >= 1e-6)
	{
		float3 proj = clip.xyz / w;
		if (proj.z > 0.0 && proj.z < 1.0)
		{
			float2 uvTile = proj.xy * float2(0.5, -0.5) + float2(0.5, 0.5);
			if (all(uvTile >= float2(0.0, 0.0)) && all(uvTile <= float2(1.0, 1.0)))
				outVis = clamp(ComputeShadowPCSSAtlasTile(uvTile, (float)idx, CameraForwardInvCount.w, proj.z, normal, worldPos), 0.0, 1.0);
		}
	}
	return outVis;
}

float PrimaryDirectionalShadowVisForIBL(float3 worldPos, float3 normal)
{
	float vis = 1.0;
	if (IsEnableShadow())
		vis = clamp(DirectionalShadowVisibility(worldPos, normal), 0.0, 1.0);
	return vis;
}

#endif // MINIENGINE_DIRECTIONAL_SHADOW_HLSL
