// Lightweight directional shadow for TranslucentPBRForward only (no PCSS / no blocker search).
// Include after: PBRMaterialSampling, ShadowMap (t8), SampleShadow (s1), ShadowCompareSampler (s2), cbDirectionalShadowCSM (b7).
// Provides: kPoissonDisk16 (for SpotShadowSampling), ComputeShadowPCSS (for DeferredLightingShared.ComputeShadow),
// DirectionalShadowVisibility, PrimaryDirectionalShadowVisForIBL.
//
// Trade: stable PCF + cascade selection matches full path layout; softer penumbra than PCSS, much faster FXC compile.

#ifndef MINIENGINE_TRANSLUCENT_SHADOW_LITE_HLSL
#define MINIENGINE_TRANSLUCENT_SHADOW_LITE_HLSL

static const float2 kPoissonDisk16[16] =
{
	float2(-0.94201624, -0.39906216), float2(0.94558609, -0.76890725), float2(-0.09418410, -0.92938870),
	float2(0.34495938, 0.29387760), float2(-0.91588581, 0.45771432), float2(-0.81544232, -0.87912464),
	float2(-0.38277543, 0.27676845), float2(0.97484398, 0.75648379), float2(0.44323325, -0.97511554),
	float2(0.53742981, -0.47373420), float2(-0.26496911, -0.41893023), float2(0.79197514, 0.19090188),
	float2(-0.24188840, 0.99706507), float2(-0.81409955, 0.91437590), float2(0.19984126, 0.78641367),
	float2(0.14383161, -0.14100790)
};

static const float kTranslucentDirPcfRadiusFull = 0.00105;
static const float kTranslucentDirPcfRadiusCascade = 0.00155;

float ShadowDepthBiasTranslucent(float3 Normal)
{
	float3 L = normalize(GetMainLight().Direction);
	float3 n = normalize(Normal);
	float NdotL = abs(dot(n, L));
	const float baseBias = 0.00038;
	const float slopeBias = 0.00135;
	float grazing = saturate(1.0 - abs(L.y));
	const float horizonBias = 0.0024 * grazing;
	return baseBias + slopeBias * (1.0 - NdotL) + horizonBias;
}

float ShadowDepthBiasTranslucent_Cascade(float3 Normal)
{
	return ShadowDepthBiasTranslucent(Normal) + 0.00055 + (1.0 - abs(dot(normalize(Normal), normalize(GetMainLight().Direction)))) * 0.00085;
}

float TranslucentPCF_FullAtlas(float2 uv01, float zReceiver, float radiusUV, float bias)
{
	const float ref = zReceiver - bias;
	float lit = 0.0;
	[unroll]
	for (int i = 0; i < 9; ++i)
	{
		int ox = (i % 3) - 1;
		int oy = (i / 3) - 1;
		float2 suv = uv01 + float2((float)ox, (float)oy) * radiusUV;
		suv = clamp(suv, float2(1e-4, 1e-4), float2(1.0 - 1e-4, 1.0 - 1e-4));
		lit += ShadowMap.SampleCmpLevelZero(ShadowCompareSampler, suv, ref);
	}
	return lit * (1.0 / 9.0);
}

float TranslucentPCF_CascadeTile(float2 uvTile01, float cascadeIdx, float invN, float zReceiver, float3 Normal, float radiusUV)
{
	float result = 1.0;
	if (zReceiver > 0.0 && zReceiver < 1.0 && all(uvTile01 >= float2(0.0, 0.0)) && all(uvTile01 <= float2(1.0, 1.0)))
	{
		float2 uvAtlas = float2(uvTile01.x, (cascadeIdx + uvTile01.y) * invN);
		float bias = ShadowDepthBiasTranslucent_Cascade(Normal);
		const float ref = zReceiver - bias;
		float lit = 0.0;
		[unroll]
		for (int i = 0; i < 9; ++i)
		{
			int ox = (i % 3) - 1;
			int oy = (i / 3) - 1;
			float2 ofs = float2((float)ox * radiusUV, (float)oy * radiusUV * invN);
			float2 suv = uvAtlas + ofs;
			suv = clamp(suv, float2(1e-4, 1e-4), float2(1.0 - 1e-4, 1.0 - 1e-4));
			lit += ShadowMap.SampleCmpLevelZero(ShadowCompareSampler, suv, ref);
		}
		result = lit * (1.0 / 9.0);
	}
	return result;
}

float DirectionalShadowVisSampleCascade(float3 worldPos, float3 normal, int ci)
{
	row_major matrix vp = CascadeViewProj[0];
	if (ci == 1)
		vp = CascadeViewProj[1];
	else if (ci == 2)
		vp = CascadeViewProj[2];

	float outVis = 1.0;
	float4 clip = mul(float4(worldPos, 1.0), vp);
	float w = clip.w;
	if (abs(w) >= 1e-6)
	{
		float3 proj = clip.xyz / w;
		if (proj.z > 0.0 && proj.z < 1.0)
		{
			float2 uvTile = proj.xy * float2(0.5, -0.5) + float2(0.5, 0.5);
			if (all(uvTile >= float2(0.0, 0.0)) && all(uvTile <= float2(1.0, 1.0)))
			{
				float invN = CameraForwardInvCount.w;
				outVis = clamp(TranslucentPCF_CascadeTile(uvTile, (float)ci, invN, proj.z, normal, kTranslucentDirPcfRadiusCascade), 0.0, 1.0);
			}
		}
	}
	return outVis;
}

float DirectionalShadowVisibility(float3 worldPos, float3 normal)
{
	float outVis = 1.0;
	if (DirectionalCSMEnabled == 0)
	{
		float4 clip = mul(float4(worldPos, 1.0), GetMainLightViewProj());
		float w = clip.w;
		if (abs(w) >= 1e-6)
		{
			float3 proj = clip.xyz / w;
			if (proj.z > 0.0 && proj.z < 1.0)
			{
				float2 uvTile = proj.xy * float2(0.5, -0.5) + float2(0.5, 0.5);
				if (all(uvTile >= float2(0.0, 0.0)) && all(uvTile <= float2(1.0, 1.0)))
				{
					float invN = CameraForwardInvCount.w;
					if (invN < 1e-5)
						invN = 0.33333334;
					const float zR = clamp(proj.z, 0.0, 1.0);
					outVis = clamp(TranslucentPCF_CascadeTile(uvTile, 0.0, invN, zR, normal, kTranslucentDirPcfRadiusCascade), 0.0, 1.0);
				}
			}
		}
	}
	else
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

float PrimaryDirectionalShadowVisForIBL(float3 worldPos, float3 normal)
{
	float vis = 1.0;
	if (IsEnableShadow())
		vis = clamp(DirectionalShadowVisibility(worldPos, normal), 0.0, 1.0);
	return vis;
}

/** Matches ShadowPCSS.hlsl entry point used by DeferredLightingShared.ComputeShadow (single atlas, no CSM tile). */
float ComputeShadowPCSS(float4 ShadowCoord, float3 Normal)
{
	float outVis = 1.0;
	const float w = ShadowCoord.w;
	if (abs(w) >= 1e-6)
	{
		const float3 proj = ShadowCoord.xyz / w;
		if (proj.z > 0.0 && proj.z < 1.0)
		{
			const float2 uv = proj.xy * float2(0.5, -0.5) + float2(0.5, 0.5);
			if (all(uv >= float2(0.0, 0.0)) && all(uv <= float2(1.0, 1.0)))
			{
				const float zR = clamp(proj.z, 0.0, 1.0);
				const float bias = ShadowDepthBiasTranslucent(Normal);
				outVis = TranslucentPCF_FullAtlas(uv, zR, kTranslucentDirPcfRadiusFull, bias);
			}
		}
	}
	return outVis;
}

#endif // MINIENGINE_TRANSLUCENT_SHADOW_LITE_HLSL
