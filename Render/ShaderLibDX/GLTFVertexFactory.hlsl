#include "Skinning.hlsl"
#include "GLTFPbrPass-IO.hlsl"
#include "PerFrameStruct.hlsl"

// ATTRIBUTE indices match PBRMaterialRender / FShadowPassMeshDraw / FurMaterialRender InitShader:
// 0–2 pos/normal/UV, 3 tangent (always), 4–5 joints only when ID_SKINNING_MATRICES.
struct VS_INPUT_SCENE
{
	float3 Position : ATTRIBUTE0;
	float3 Normal : ATTRIBUTE1;
	float2 UV0 : ATTRIBUTE2;
	float4 Tangent : ATTRIBUTE3;
#ifdef ID_SKINNING_MATRICES
	float4 JointsWeights0 : ATTRIBUTE4;
	float4 JointsIndices0 : ATTRIBUTE5;
#endif
};

VS_OUTPUT_SCENE gltfVertexFactory(VS_INPUT_SCENE input, uint InstanceId : SV_InstanceID)
{
	VS_OUTPUT_SCENE Output = (VS_OUTPUT_SCENE)0;
#ifdef ID_SKINNING_MATRICES
	matrix skinningMatrix = GetCurrentSkinningMatrix(input.JointsWeights0, input.JointsIndices0);
#else
	matrix skinningMatrix = matrix(
		float4(1, 0, 0, 0),
		float4(0, 1, 0, 0),
		float4(0, 0, 1, 0),
		float4(0, 0, 0, 1));
#endif

	matrix transMatrix = mul(skinningMatrix, GetWorldMatrix());
	Output.Normal = normalize(mul(float4(input.Normal, 0), transMatrix).xyz);
	Output.UV0 = input.UV0;
	Output.WorldPos = mul(float4(input.Position, 1.0f), transMatrix).xyz;

	Output.svPosition = mul(float4(Output.WorldPos, 1.0f), GetCameraViewProj());
	if (IsEnableShadow())
	{
		Output.LightPos = mul(float4(Output.WorldPos, 1.0f), GetMainLightViewProj());
	}

#ifdef ID_SKINNING_MATRICES
	matrix prevSkinningMatrix = GetPreviousSkinningMatrix(input.JointsWeights0, input.JointsIndices0);
#else
	matrix prevSkinningMatrix = matrix(
		float4(1, 0, 0, 0),
		float4(0, 1, 0, 0),
		float4(0, 0, 1, 0),
		float4(0, 0, 0, 1));
#endif

	Output.svCurrPosition = Output.svPosition;

	matrix prevTransMatrix = mul(prevSkinningMatrix, GetPrevWorldMatrix());
	float3 worldPrevPos = mul(float4(input.Position, 1), prevTransMatrix).xyz;
	Output.svPrevPosition = mul(float4(worldPrevPos, 1), GetPrevCameraViewProj());

	float3 T = normalize(mul(float4(input.Tangent.xyz, 0.0), transMatrix).xyz);
	float3 N = Output.Normal;
	T = normalize(T - N * dot(N, T));
	float handedness = (input.Tangent.w >= 0.0) ? 1.0 : -1.0;
	float3 B = cross(N, T) * handedness;
	Output.Tangent = T;
	Output.Binormal = B;
	return Output;
}

VS_OUTPUT_SCENE gltfVertexFactoryForLight(VS_INPUT_SCENE input)
{
	VS_OUTPUT_SCENE Output = (VS_OUTPUT_SCENE)0;
#ifdef ID_SKINNING_MATRICES
	matrix skinningMatrix = GetCurrentSkinningMatrix(input.JointsWeights0, input.JointsIndices0);
#else
	matrix skinningMatrix = matrix(
		float4(1, 0, 0, 0),
		float4(0, 1, 0, 0),
		float4(0, 0, 1, 0),
		float4(0, 0, 0, 1));
#endif

	matrix transMatrix = mul(skinningMatrix, GetWorldMatrix());
	Output.Normal = normalize(mul(float4(input.Normal, 0), transMatrix).xyz);
	Output.UV0 = input.UV0;
	Output.WorldPos = mul(float4(input.Position, 1.0f), transMatrix).xyz;

	Output.svPosition = mul(float4(Output.WorldPos, 1.0f), GetMainLightViewProj());

	float3 T = normalize(mul(float4(input.Tangent.xyz, 0.0), transMatrix).xyz);
	float3 N = Output.Normal;
	T = normalize(T - N * dot(N, T));
	float handedness = (input.Tangent.w >= 0.0) ? 1.0 : -1.0;
	float3 B = cross(N, T) * handedness;
	Output.Tangent = T;
	Output.Binormal = B;
	return Output;
}
