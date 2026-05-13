// Fullscreen deferred lighting: reads scene textures from base pass, applies analytic lights + split-sum IBL.
//
// Include / declaration order is load-bearing. Summary (do not reorder without checking dependents):
//   1) ShaderUtils, PerFrameStruct, DeferredShadingCommon — types + cbPerFrame (b0), no t/s registers.
//   2) Textures, samplers, cbPointShadow (b4).
//   3) ClusterLightLookup — t13–t15 (cluster tables shared with Forward+; deferred uses ClusterIndexFromPixelWorld).
//   4) ShadowPCSS — needs ShadowMap (t8), s0/s1/s2, GetMainLight().
//   5) DirectionalShadowCB + DirectionalShadow.hlsl.
//   6) SpotShadowSampling — kPoissonDisk16, ShadowCompareSampler; t11 + cbSpotShadow (b5).
//   7) DeferredLightingShared — IBL + point-cube PCF, ComputeShadowPCSS / hair helpers.
//   8) DeferredLightingAnalytic — punctual lights.

// --- 1. Foundation (no PS resource registers) ---
#include "ShaderUtils.hlsl"
#include "PerFrameStruct.hlsl"
#include "DeferredShadingCommon.hlsl"

// --- 2. PS resource bindings (must appear before any #include that samples or declares overlapping registers) ---
Texture2D BaseColorGBuffer : register(t0);
Texture2D NormalGBuffer : register(t1);
Texture2D EmissiveGBuffer : register(t2);
Texture2D MRGBuffer : register(t3);
Texture2D DepthTexture : register(t4);
TextureCube IrradianceTex : register(t5);
Texture2D BrdfLut : register(t6);
TextureCube PrefilterCubeMap : register(t7);
Texture2D ShadowMap : register(t8);
Texture2D MaterialAuxGBuffer : register(t9);
TextureCube PointShadowCube : register(t10);
Texture2D GroundEnvLatLong : register(t12);
SamplerState SampleLinear : register(s0);
SamplerState SampleShadow : register(s1);
SamplerComparisonState ShadowCompareSampler : register(s2);

cbuffer cbPointShadow : register(b4)
{
	row_major matrix PointFaceVP[6];
	float4 PointShadowLightPosRange; // xyz = light pos, w = range (CPU-filled; matches Engine::CBPointShadow)
	int PointShadowEnabled;
	int PointShadowLightIndex;
	uint2 PointShadowPad;
};

// --- 3. Cluster light list SRVs (same registers as clustered Forward+; must precede code that references them) ---
#include "ClusterLightLookup.hlsl"

// --- 4–6. Directional + spot shadow chain ---
#include "ShadowPCSS.hlsl"
#include "DirectionalShadowCB.hlsl"
#include "DirectionalShadow.hlsl"
#include "SpotShadowSampling.hlsl"

// --- 7–8. IBL / material + analytic punctual ---
#include "DeferredLightingShared.hlsl"
#include "DeferredLightingAnalytic.hlsl"

struct PSInput
{
	float2 Tex : TEXCOORD;
	float4 Pos : SV_Position;
};

// Fullscreen triangle; must live in this file so VS/PS share cbPerFrame at b0 (not PostProcess BloomContants).
PSInput VS_ScreenQuad(uint VertID : SV_VertexID)
{
	PSInput Out;
	float2 Tex = float2(uint2(VertID, VertID << 1) & 2);
	Out.Tex = Tex;
	Out.Pos = float4(lerp(float2(-1, 1), float2(1, -1), Tex), 0, 1);
	return Out;
}

float3 ReconstructWorldPosition(float2 uv, float depthHw)
{
	// Hardware depth in [0,1] with clear=1 (far). Row-vector clip: world * VP = clip, clip.w = view-space Z
	// for standard LH perspective. Recover clip before divide, then inv(VP) -> world (matches VS mul(world, VP)).
	float2 ndcXY = float2(uv.x * 2.0 - 1.0, 1.0 - uv.y * 2.0);
	float n = myPerFrame.CameraNearZ;
	float f = myPerFrame.CameraFarZ;
	float denom = max(f - depthHw * (f - n), 1e-6);
	float clipW = (n * f) / denom;
	float4 clipH = float4(ndcXY.x * clipW, ndcXY.y * clipW, depthHw * clipW, clipW);
	float4 w = mul(clipH, myPerFrame.CameraCurrViewProjInverse);
	return w.xyz / max(w.w, 1e-5);
}

float4 PS_DeferredLighting(PSInput Input) : SV_Target0
{
	float2 uv = Input.Tex;
	float depth = DepthTexture.Sample(SampleLinear, uv).r;
	float4 baseSample = BaseColorGBuffer.Sample(SampleLinear, uv);
	float3 baseColor = baseSample.rgb;
	float alpha = baseSample.a;

	if (depth >= 0.99999)
		return float4(baseColor, alpha);

	float3 worldPos = ReconstructWorldPosition(uv, depth);
	float3 packedN = NormalGBuffer.Sample(SampleLinear, uv).xyz;
	float3 nUnnorm = packedN * 2.0 - 1.0;
	float nLen = length(nUnnorm);
	float3 normal = (nLen > 1e-5) ? (nUnnorm / nLen) : float3(0.0, 0.0, 1.0);
	float4 emiss = EmissiveGBuffer.Sample(SampleLinear, uv);
	float4 mr = MRGBuffer.Sample(SampleLinear, uv);
	float metallic = mr.r;
	float aoRaw = mr.g;
	float perceptualRoughness = mr.b;
	// Aggressive baked AO (often 0 in crevices) drives specOcc to ~0 and removes all IBL specular. Floor for spec path only.
	const float aoDiffuse = max(aoRaw, 1e-4);
	const float aoSpec = max(aoRaw, 0.2);

	float4 auxSample = MaterialAuxGBuffer.Sample(SampleLinear, uv);
	uint smid = DecodeShadingModelId(auxSample.r);
	// Hair SM: lit only in forward fur pass (FurMaterial / FurForwardAccumulate); skip deferred to avoid double lighting.
	if (IsHairShadingModel(smid))
		return float4(baseColor + emiss.rgb, alpha);

	float3 viewVec = myPerFrame.CameraPos.xyz - worldPos;
	float vLen = length(viewVec);
	float3 view = (vLen > 1e-5) ? (viewVec / vLen) : float3(0.0, 0.0, 1.0);
	MaterialInfo materialInfo;
	DecodeMaterialFromGBuffer(baseColor, metallic, perceptualRoughness, materialInfo);

	// Fur/shell edges: SrcAlpha-blended normals between strands and contact geometry crush N·L on default-lit GGX. Nudge toward
	// world up for dielectric, high-roughness semi-transparent pixels.
	if (baseSample.a < 0.999f && baseSample.a > 1e-3f && metallic < 0.05f && perceptualRoughness > 0.5f)
	{
		float edge = saturate((1.0f - baseSample.a) * 3.0f);
		float3 upW = float3(0.0f, 1.0f, 0.0f);
		normal = normalize(lerp(normal, upW, edge * 0.45f));
	}

	float3 color = float3(0, 0, 0);

	const uint clusterIdx = ClusterIndexFromPixelWorld(Input.Pos.xy, worldPos);
	const uint2 clusterRange = _ClusterLightOffsetCount[clusterIdx];
	[loop]
	for (uint slot = 0u; slot < clusterRange.y; ++slot)
	{
		const uint li = _ClusterLightIndexList[clusterRange.x + slot];
		if (li >= (uint)myPerFrame.LightCount)
			continue;
		const int i = (int)li;
		Light light = myPerFrame.Lights[i];
		if (light.Type == LightType_Directional)
			color += ApplyDirectionalLightDeferred(worldPos, light, materialInfo, normal, view);
		else if (light.Type == LightType_Point)
			color += ApplyPointLight(light, materialInfo, normal, worldPos, view, i);
		else if (light.Type == LightType_Spot)
			color += ApplySpotLight(light, materialInfo, normal, worldPos, view, i);
	}

	float3 iblDiffuse, iblSpecular;
	GetIBLContributionSplit(materialInfo, normal, view, iblDiffuse, iblSpecular);
	float NdotVao = saturate(dot(normal, view));
	float specOccPowBase = max(NdotVao + aoSpec - 0.0001, 1e-5);
	float specOcc = saturate(pow(specOccPowBase, exp2(-14.0 * perceptualRoughness - 0.62)) - 1.0 + aoSpec);

	const float coupleD = saturate(myPerFrame.IBLDirShadowCoupling.x);
	const float coupleS = saturate(myPerFrame.IBLDirShadowCoupling.y);
	float iblDiffScale = 1.0;
	float iblSpecScale = 1.0;
	if (coupleD > 0.0 || coupleS > 0.0)
	{
		const float dirVisIBL = PrimaryDirectionalShadowVisForIBL(worldPos, normal);
		iblDiffScale = lerp(1.0, dirVisIBL, coupleD);
		iblSpecScale = lerp(1.0, dirVisIBL, coupleS);
	}
	const float iblAoExp = max(myPerFrame.IBLDirShadowCoupling.z, 1e-3);
	const float aoForIblDiffuse = pow(max(aoDiffuse, 1e-4), iblAoExp);
	color += (iblDiffuse * aoForIblDiffuse * iblDiffScale + iblSpecular * specOcc * iblSpecScale) * myPerFrame.IBLFactor;

	color += emiss.rgb;

	return float4(color, alpha);
}
