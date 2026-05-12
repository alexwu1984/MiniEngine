// Directional CSM sampling for deferred / forward paths. Include after ShadowPCSS.hlsl and cbDirectionalShadowCSM (register b7).
// ComputeShadowHairCascadeAtlas is omitted when MINIENGINE_DEFERRED_LIGHTING_SKIP_HAIR is defined before this include.

#ifndef MINIENGINE_DIRECTIONAL_SHADOW_CSM_HLSL
#define MINIENGINE_DIRECTIONAL_SHADOW_CSM_HLSL

/** PCSS visibility for one CSM cascade index (0..2). Returns 1 if outside the cascade frustum tile. */
float DirectionalShadowVisSampleCascade(float3 worldPos, float3 normal, int ci)
{
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
	if (any(uvTile < float2(0.0, 0.0)) || any(uvTile > float2(1.0, 1.0)))
		return 1.0;
	float invN = CameraForwardInvCount.w;
	return clamp(ComputeShadowPCSSCascadeTile(uvTile, (float)ci, invN, proj.z, normal, worldPos), 0.0, 1.0);
}

float DirectionalShadowVisibility(float3 worldPos, float3 normal)
{
	float outVis = 1.0;
	if (DirectionalCSMEnabled == 0)
	{
		// Single-map directional is rendered into atlas tile 0 (square 2048², same layout as CSM cascade 0).
		// Sample with ComputeShadowPCSSCascadeTile (rotated Poisson + per-tile UV) — full-atlas ComputeShadowPCSS
		// stretched kernels in V and caused strong vertical banding on floors.
		float4 clip = mul(float4(worldPos, 1.0), GetMainLightViewProj());
		float w = clip.w;
		if (abs(w) < 1e-6)
			return 1.0;
		float3 proj = clip.xyz / w;
		if (proj.z <= 0.0 || proj.z >= 1.0)
			return 1.0;
		float2 uvTile = proj.xy * float2(0.5, -0.5) + float2(0.5, 0.5);
		if (any(uvTile < float2(0.0, 0.0)) || any(uvTile > float2(1.0, 1.0)))
			return 1.0;
		float invN = CameraForwardInvCount.w;
		if (invN < 1e-5)
			invN = 0.33333334;
		outVis = clamp(ComputeShadowPCSSCascadeTile(uvTile, 0.0, invN, proj.z, normal, worldPos), 0.0, 1.0);
	}
	else
	{
		const float ze = dot(worldPos - myPerFrame.CameraPos.xyz, CameraForwardInvCount.xyz);
		const float s0 = CascadeSplits.x;
		const float s1 = CascadeSplits.y;
		const float camNear = myPerFrame.CameraNearZ;
		const float span0 = max(s0 - camNear, 1e-2);
		const float span1 = max(s1 - s0, 1e-2);
		// Wider blend = smoother splits (cost: up to 2 PCSS evals in overlap bands only).
		static const float kCascadeBlendFrac = 0.28;
		const float B0 = span0 * kCascadeBlendFrac;
		const float B1 = span1 * kCascadeBlendFrac;

		if (ze < s0 - B0)
			outVis = DirectionalShadowVisSampleCascade(worldPos, normal, 0);
		else if (ze < s0 + B0)
		{
			const float v0 = DirectionalShadowVisSampleCascade(worldPos, normal, 0);
			const float v1 = DirectionalShadowVisSampleCascade(worldPos, normal, 1);
			outVis = lerp(v0, v1, smoothstep(s0 - B0, s0 + B0, ze));
		}
		else if (ze < s1 - B1)
			outVis = DirectionalShadowVisSampleCascade(worldPos, normal, 1);
		else if (ze < s1 + B1)
		{
			const float v1 = DirectionalShadowVisSampleCascade(worldPos, normal, 1);
			const float v2 = DirectionalShadowVisSampleCascade(worldPos, normal, 2);
			outVis = lerp(v1, v2, smoothstep(s1 - B1, s1 + B1, ze));
		}
		else
			outVis = DirectionalShadowVisSampleCascade(worldPos, normal, 2);
	}
	return outVis;
}

#ifndef MINIENGINE_DEFERRED_LIGHTING_SKIP_HAIR
float HairCascadeVisOne(int ci, float3 worldPos, float3 Normal, float coverageAlpha)
{
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
	if (any(uvTile < float2(0.0, 0.0)) || any(uvTile > float2(1.0, 1.0)))
		return 1.0;
	float zR = clamp(proj.z, 0.0, 1.0);
	float bias = ShadowDepthBiasPCSS_Cascade(Normal);
	bias += 0.00115 + saturate(1.0 - coverageAlpha) * 0.00135;
	float invN = CameraForwardInvCount.w;
	float2 uvAtlas = float2(uvTile.x, ((float)ci + uvTile.y) * invN);
	const float fixedPcfRadius = 0.0031;
	float rotAng = 6.28318530718 * frac(
		0.6180339887 * dot(worldPos.xz, float2(0.724, 0.311)) + 0.381 * worldPos.y + (float)ci * 0.271828 + zR * 3.14159);
	float sa, ca;
	sincos(rotAng, sa, ca);

	float lit = 0.0;
	const float ref = zR - bias;
	[unroll]
	for (int i = 0; i < 16; ++i)
	{
		float2 disk = kPoissonDisk16[i];
		float2 rd = float2(ca * disk.x - sa * disk.y, sa * disk.x + ca * disk.y);
		float2 diskOfs = float2(rd.x, rd.y * invN) * fixedPcfRadius;
		float2 suv = uvAtlas + diskOfs;
		suv = clamp(suv, float2(1e-4, 1e-4), float2(1.0 - 1e-4, 1.0 - 1e-4));
		lit += ShadowMap.SampleCmpLevelZero(ShadowCompareSampler, suv, ref);
	}
	return lit * (1.0 / 16.0);
}

float ComputeShadowHairCascadeAtlas(float3 worldPos, float3 Normal, float coverageAlpha)
{
	const float ze = dot(worldPos - myPerFrame.CameraPos.xyz, CameraForwardInvCount.xyz);
	const float s0 = CascadeSplits.x;
	const float s1 = CascadeSplits.y;
	const float camNear = myPerFrame.CameraNearZ;
	const float span0 = max(s0 - camNear, 1e-2);
	const float span1 = max(s1 - s0, 1e-2);
	static const float kCascadeBlendFrac = 0.28;
	const float B0 = span0 * kCascadeBlendFrac;
	const float B1 = span1 * kCascadeBlendFrac;

	if (ze < s0 - B0)
		return HairCascadeVisOne(0, worldPos, Normal, coverageAlpha);
	if (ze < s0 + B0)
	{
		const float v0 = HairCascadeVisOne(0, worldPos, Normal, coverageAlpha);
		const float v1 = HairCascadeVisOne(1, worldPos, Normal, coverageAlpha);
		return lerp(v0, v1, smoothstep(s0 - B0, s0 + B0, ze));
	}
	if (ze < s1 - B1)
		return HairCascadeVisOne(1, worldPos, Normal, coverageAlpha);
	if (ze < s1 + B1)
	{
		const float v1 = HairCascadeVisOne(1, worldPos, Normal, coverageAlpha);
		const float v2 = HairCascadeVisOne(2, worldPos, Normal, coverageAlpha);
		return lerp(v1, v2, smoothstep(s1 - B1, s1 + B1, ze));
	}
	return HairCascadeVisOne(2, worldPos, Normal, coverageAlpha);
}
#endif // !MINIENGINE_DEFERRED_LIGHTING_SKIP_HAIR

/** [0,1] sun visibility for coupling skylight IBL into directional shadow (1 when shadow off / invalid). */
float PrimaryDirectionalShadowVisForIBL(float3 worldPos, float3 normal)
{
	float vis = 1.0;
	if (IsEnableShadow())
		vis = clamp(DirectionalShadowVisibility(worldPos, normal), 0.0, 1.0);
	return vis;
}

#endif // MINIENGINE_DIRECTIONAL_SHADOW_CSM_HLSL
