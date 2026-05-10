// Fullscreen deferred lighting: reads scene textures from base pass, applies analytic lights + split-sum IBL.
#include "ShaderUtils.hlsl"
#include "PerFrameStruct.hlsl"
#include "DeferredShadingCommon.hlsl"

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

#include "ShadowPCSS.hlsl"

cbuffer cbPointShadow : register(b4)
{
    row_major matrix PointFaceVP[6];
    float4 PointShadowLightPosRange; // xyz = light pos, w = range (CPU-filled; matches Engine::CBPointShadow)
    int PointShadowEnabled;
    int PointShadowLightIndex;
    uint2 PointShadowPad;
};

#include "SpotShadowSampling.hlsl"

#include "DeferredLightingShared.hlsl"

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

#include "DeferredLightingAnalytic.hlsl"

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
	int mdi = myPerFrame.PrimaryDirectionalLightIndex;
	// One directional shadow map (t8): DeferredLightingPass.cpp merges VP only into PrimaryDirectionalLightIndex and forces ShadowMapIndex=-1 on other directionals.
	float4 mainLightClip = float4(0, 0, 0, 1);
	if (mdi >= 0 && mdi < MAX_LIGHT_INSTANCES && myPerFrame.Lights[mdi].ShadowMapIndex >= 0)
		mainLightClip = mul(float4(worldPos, 1.0), myPerFrame.Lights[mdi].LightViewProj);

	[loop]
	for (int i = 0; i < myPerFrame.LightCount; ++i)
	{
		Light light = myPerFrame.Lights[i];
		if (light.Type == LightType_Directional)
		{
			float4 lc = (light.ShadowMapIndex >= 0) ? mainLightClip : float4(0, 0, 0, 1);
			color += ApplyDirectionalLightDeferred(lc, light, materialInfo, normal, view);
		}
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

	color += (iblDiffuse * aoDiffuse + iblSpecular * specOcc) * myPerFrame.IBLFactor;

	color += emiss.rgb;

	return float4(color, alpha);
}
