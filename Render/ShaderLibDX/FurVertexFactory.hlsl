#include "Skinning.hlsl"
#include "FurPass-IO.hlsl"
#include "PerFrameStruct.hlsl"

// ATTRIBUTE indices match FurMaterialRender / PBRMaterialRender InitShader (fur shell uses same IL as PBR).
struct VS_INPUT_FUR
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

VS_OUTPUT_FUR furVertexFactory(VS_INPUT_FUR input, uint InstanceId : SV_InstanceID)
{
	VS_OUTPUT_FUR Output = (VS_OUTPUT_FUR)0;
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

	// Instanced shells: one draw with FurLevel instances; layer index from SV_InstanceID when FurLevel >= 1.
	float shellFurOffset = FurOffset;
	if (FurLevel >= 0.5)
		shellFurOffset = ((float)InstanceId + 1.0) / max(FurLevel, 1.0);

	float2 UVoffset = float2(0.2, 0.2) * shellFurOffset;
	UVoffset *= 0.1;
	Output.UV0 = input.UV0 * UVScale + UVoffset;
	Output.UV1 = input.UV0;
	const float furLength_coeff = 1.0;
	const float vGravityStength = 0.5;
	float3 Direction = lerp(input.Normal, Gravity * vGravityStength + input.Normal * (1.0 - vGravityStength), shellFurOffset);
	float3 P = input.Position + Direction * FurLength * shellFurOffset * furLength_coeff;
	Output.WorldPos = mul(float4(P, 1.0f), transMatrix).xyz;

	float SH = clamp(Output.Normal.y * 0.25 + 0.35, 0.0, 1.0);
	Output.SH = float3(SH, SH, SH);
	Output.FurShellOffset = shellFurOffset;

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
