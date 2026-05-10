// Directional CSM sampling for deferred / forward paths. Include after ShadowPCSS.hlsl and cbDirectionalShadowCSM (register b7).

#ifndef MINIENGINE_DIRECTIONAL_SHADOW_CSM_HLSL
#define MINIENGINE_DIRECTIONAL_SHADOW_CSM_HLSL

float DirectionalShadowVisibility(float3 worldPos, float3 normal)
{
	float4 coord = mul(float4(worldPos, 1.0), GetMainLightViewProj());
	if (DirectionalCSMEnabled == 0)
		return clamp(ComputeShadowPCSS(coord, normal), 0.0, 1.0);

	float ze = dot(worldPos - myPerFrame.CameraPos.xyz, CameraForwardInvCount.xyz);
	int ci = 2;
	if (ze < CascadeSplits.y)
		ci = 1;
	if (ze < CascadeSplits.x)
		ci = 0;

	row_major matrix vp = CascadeViewProj[0];
	if (ci == 1)
		vp = CascadeViewProj[1];
	else if (ci == 2)
		vp = CascadeViewProj[2];

	float4 clip = mul(float4(worldPos, 1.0), vp);
	float w = clip.w;
	if (abs(w) < 1e-6)
		return 1.0;
	float3 proj = clip.xyz / w;
	if (proj.z <= 0.0 || proj.z >= 1.0)
		return 1.0;
	float2 uvTile = proj.xy * float2(0.5, -0.5) + float2(0.5, 0.5);
	if (any(uvTile < 0.0) || any(uvTile > 1.0))
		return 1.0;

	float invN = CameraForwardInvCount.w;
	return clamp(ComputeShadowPCSSCascadeTile(uvTile, (float)ci, invN, proj.z, normal), 0.0, 1.0);
}

float ComputeShadowHairCascadeAtlas(float3 worldPos, float3 Normal, float coverageAlpha)
{
	float ze = dot(worldPos - myPerFrame.CameraPos.xyz, CameraForwardInvCount.xyz);
	int ci = 2;
	if (ze < CascadeSplits.y)
		ci = 1;
	if (ze < CascadeSplits.x)
		ci = 0;

	row_major matrix vp = CascadeViewProj[0];
	if (ci == 1)
		vp = CascadeViewProj[1];
	else if (ci == 2)
		vp = CascadeViewProj[2];

	float4 clip = mul(float4(worldPos, 1.0), vp);
	float wc = clip.w;
	if (abs(wc) < 1e-6)
		return 1.0;
	float3 proj = clip.xyz / wc;
	if (proj.z <= 0.0 || proj.z >= 1.0)
		return 1.0;
	float2 uvTile = proj.xy * float2(0.5, -0.5) + float2(0.5, 0.5);
	if (any(uvTile < 0.0) || any(uvTile > 1.0))
		return 1.0;

	float zR = clamp(proj.z, 0.0, 1.0);
	float bias = ShadowDepthBiasPCSS(Normal);
	bias += 0.00115 + saturate(1.0 - coverageAlpha) * 0.00135;
	float invN = CameraForwardInvCount.w;
	float2 uvAtlas = float2(uvTile.x, ((float)ci + uvTile.y) * invN);
	const float fixedPcfRadius = 0.0020;

	float lit = 0.0;
	[unroll]
	for (int i = 0; i < 16; ++i)
	{
		float2 diskOfs = float2(kPoissonDisk16[i].x, kPoissonDisk16[i].y * invN) * fixedPcfRadius;
		float2 suv = uvAtlas + diskOfs;
		suv = clamp(suv, float2(1e-4, 1e-4), float2(1.0 - 1e-4, 1.0 - 1e-4));
		float d = ShadowMap.SampleLevel(SampleShadow, suv, 0.0).r;
		lit += (zR <= d + bias) ? 1.0 : 0.0;
	}
	return lit * (1.0 / 16.0);
}

#endif // MINIENGINE_DIRECTIONAL_SHADOW_CSM_HLSL
