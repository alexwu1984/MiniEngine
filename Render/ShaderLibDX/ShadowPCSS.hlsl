// PCSS for directional shadow map (depth atlas in [0,1] clip z space). Blocker search uses raw depth (SampleShadow);
// PCF visibility uses hardware comparison filtering (ShadowCompareSampler + SampleCmpLevelZero).
// Expects Texture2D ShadowMap, SamplerState SampleShadow, SamplerComparisonState ShadowCompareSampler, GetMainLight() from PerFrameStruct.hlsl.

#ifndef MINIENGINE_SHADOW_PCSS_HLSL
#define MINIENGINE_SHADOW_PCSS_HLSL

static const float kPCSSBlockerSearchRadiusUV = 0.010;
// Wider minimum PCF radius reduces shimmering / moire on large receivers (e.g. dense tessellated floors).
static const float kPCSSMinFilterRadiusUV = 0.00065;
static const float kPCSSMaxFilterRadiusUV = 0.0045;
static const float kPCSSPenumbraMul = 14.0;
// Full-atlas single directional (no CSM): same UV kernel covers more world space than per-cascade tiles — tame penumbra
// or contact shadows and high-tess floors read as huge average blocker distance and "melt" (worse after FXAA).
static const float kPCSSMinFilterRadiusUV_SingleMap = 0.00052f;
static const float kPCSSMaxFilterRadiusUV_SingleMap = 0.0028f;
static const float kPCSSPenumbraMul_SingleMap = 7.5f;

static const float2 kPoissonDisk16[16] =
{
	float2(-0.94201624, -0.39906216), float2(0.94558609, -0.76890725), float2(-0.09418410, -0.92938870),
	float2(0.34495938, 0.29387760), float2(-0.91588581, 0.45771432), float2(-0.81544232, -0.87912464),
	float2(-0.38277543, 0.27676845), float2(0.97484398, 0.75648379), float2(0.44323325, -0.97511554),
	float2(0.53742981, -0.47373420), float2(-0.26496911, -0.41893023), float2(0.79197514, 0.19090188),
	float2(-0.24188840, 0.99706507), float2(-0.81409955, 0.91437590), float2(0.19984126, 0.78641367),
	float2(0.14383161, -0.14100790)
};

float ShadowDepthBiasPCSS(float3 Normal)
{
	float3 L = normalize(GetMainLight().Direction);
	float3 n = normalize(Normal);
	float NdotL = abs(dot(n, L));
	const float baseBias = 0.00038;
	const float slopeBias = 0.00135;
	// Direction is toward the light; |L.y| small near horizon → grazing rays → depth aliasing / false umbra.
	float grazing = saturate(1.0 - abs(L.y));
	const float horizonBias = 0.0024 * grazing;
	return baseBias + slopeBias * (1.0 - NdotL) + horizonBias;
}

/** Stronger bias for vertical CSM atlas + PCSS: same stripe root as contact acne; UE-style is extra const + slope on receivers. */
float ShadowDepthBiasPCSS_Cascade(float3 Normal)
{
	const float b = ShadowDepthBiasPCSS(Normal);
	const float extra = 0.00055 + (1.0 - abs(dot(normalize(Normal), normalize(GetMainLight().Direction)))) * 0.00085;
	return b + extra;
}

float PCF_ShadowHardware(float2 uvCenter, float zReceiver, float radiusUV, float bias)
{
	const float ref = zReceiver - bias;
	float lit = 0.0;
	[unroll]
	for (int i = 0; i < 16; ++i)
	{
		float2 suv = uvCenter + kPoissonDisk16[i] * radiusUV;
		suv = clamp(suv, float2(1e-4, 1e-4), float2(1.0 - 1e-4, 1.0 - 1e-4));
		lit += ShadowMap.SampleCmpLevelZero(ShadowCompareSampler, suv, ref);
	}
	return lit * (1.0 / 16.0);
}

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
				const float bias = ShadowDepthBiasPCSS(Normal);
				float3 Lsun = normalize(GetMainLight().Direction);
				float grazingSun = saturate(1.0 - abs(Lsun.y));

				float sumBlocker = 0.0;
				float cnt = 0.0;
				[unroll]
				for (int j = 0; j < 16; ++j)
				{
					float2 suv = uv + kPoissonDisk16[j] * kPCSSBlockerSearchRadiusUV;
					suv = clamp(suv, float2(1e-4, 1e-4), float2(1.0 - 1e-4, 1.0 - 1e-4));
					float d = ShadowMap.SampleLevel(SampleShadow, suv, 0.0).r;
					if (d < zR - bias)
					{
						sumBlocker += d;
						cnt += 1.0;
					}
				}

				float filterUV = kPCSSMinFilterRadiusUV_SingleMap + grazingSun * 0.00055;
				if (cnt >= 1.0)
				{
					float avgB = sumBlocker / cnt;
					float pen = saturate((zR - avgB) * kPCSSPenumbraMul_SingleMap);
					filterUV = lerp(kPCSSMinFilterRadiusUV_SingleMap, kPCSSMaxFilterRadiusUV_SingleMap, pen);
				}

				outVis = PCF_ShadowHardware(uv, zR, filterUV, bias);
			}
		}
	}
	return outVis;
}

/** Vertical atlas: cascade `ci` occupies v in [ci/N, (ci+1)/N). `worldPos` rotates the Poisson disk to break fixed 4x4-ish block patterns (motorcycle / drone scenes). */
float ComputeShadowPCSSCascadeTile(float2 uvTile01, float cascadeIdx, float invN, float zReceiver, float3 Normal, float3 worldPos)
{
	float outVis = 1.0;
	if (zReceiver > 0.0 && zReceiver < 1.0 && all(uvTile01 >= float2(0.0, 0.0)) && all(uvTile01 <= float2(1.0, 1.0)))
	{
		float2 uvAtlas = float2(uvTile01.x, (cascadeIdx + uvTile01.y) * invN);
		const float bias = ShadowDepthBiasPCSS_Cascade(Normal);
		float3 Lsun = normalize(GetMainLight().Direction);
		float grazingSun = saturate(1.0 - abs(Lsun.y));
		// CSM tiles are 1/N atlas height: same absolute UV kernel covers fewer texels in Y → harsher blockiness without wider min filter.
		static const float kCascadePCSSMinMul = 2.75;
		static const float kCascadePCSSBlockerMul = 1.35;
		static const float kCascadePCSSMaxMul = 1.12;
		// Lower than kPCSSPenumbraMul: wide penumbra on contact + tall atlas exaggerates horizontal banding (many engines cap PCSS on dir-CSM).
		static const float kCascadePenumbraMul = 8.5;
		const float minF = kPCSSMinFilterRadiusUV * kCascadePCSSMinMul;
		const float maxF = kPCSSMaxFilterRadiusUV * kCascadePCSSMaxMul;
		const float blockerR = kPCSSBlockerSearchRadiusUV * kCascadePCSSBlockerMul;
		// Per-pixel rotation in tile space (then squash V by invN for atlas), breaks screen-aligned Poisson moiré.
		float rotAng = 6.28318530718 * frac(
			0.6180339887 * dot(worldPos.xz, float2(0.724, 0.311)) + 0.381 * worldPos.y + cascadeIdx * 0.271828 + zReceiver * 3.14159);
		float sa, ca;
		sincos(rotAng, sa, ca);

		float sumBlocker = 0.0;
		float cnt = 0.0;
		[unroll]
		for (int j = 0; j < 16; ++j)
		{
			float2 disk = kPoissonDisk16[j];
			float2 rd = float2(ca * disk.x - sa * disk.y, sa * disk.x + ca * disk.y);
			float2 ofs = float2(rd.x, rd.y * invN) * blockerR;
			float2 suv = uvAtlas + ofs;
			suv = clamp(suv, float2(1e-4, 1e-4), float2(1.0 - 1e-4, 1.0 - 1e-4));
			float dmap = ShadowMap.SampleLevel(SampleShadow, suv, 0.0).r;
			if (dmap < zReceiver - bias)
			{
				sumBlocker += dmap;
				cnt += 1.0;
			}
		}

		float filterUV = minF + grazingSun * 0.00155;
		if (cnt >= 1.0)
		{
			float avgB = sumBlocker / cnt;
			float pen = saturate((zReceiver - avgB) * kCascadePenumbraMul);
			filterUV = lerp(minF, maxF, pen);
			// Contact / near-contact: PCSS blocker average is noisy on tessellated floors → variable huge kernel → stripes. Lock to tight PCF.
			static const float kContactPenCap = 0.22;
			if (pen < kContactPenCap)
				filterUV = lerp(filterUV, minF * 1.35, saturate((kContactPenCap - pen) / kContactPenCap));
		}

		float2 filterOfsScale = float2(1.0, invN);
		const float ref = zReceiver - bias;
		float lit = 0.0;
		[unroll]
		for (int i = 0; i < 16; ++i)
		{
			float2 disk = kPoissonDisk16[i];
			float2 rd = float2(ca * disk.x - sa * disk.y, sa * disk.x + ca * disk.y);
			float2 suv = uvAtlas + rd * filterOfsScale * filterUV;
			suv = clamp(suv, float2(1e-4, 1e-4), float2(1.0 - 1e-4, 1.0 - 1e-4));
			lit += ShadowMap.SampleCmpLevelZero(ShadowCompareSampler, suv, ref);
		}
		outVis = lit * (1.0 / 16.0);
	}
	return outVis;
}

#endif // MINIENGINE_SHADOW_PCSS_HLSL
