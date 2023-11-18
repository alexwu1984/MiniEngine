#include "Skinning.hlsl"
#include "GLTFPbrPass-IO.hlsl"
#include "PerFrameStruct.hlsl"

//struct VS_INPUT_SCENE
//{
//	float3 Position : POSITION; // vertex position
//	float3 Normal : NORMAL; // this normal comes in per-vertex
//	float2 UV0 : TEXCOORD0;   // vertex texture coords
//#ifdef HAS_TANGENT
//    float4 Tangent: TANGENT; // this normal comes in per-vertex
//#endif
//#ifdef HAS_WEIGHTS_0
//    float4 JointsWeights0  : BLENDWEIGHT; //joints weights
//	float4 JointsIndices0  : BLENDINDICES; // joints indices
//#endif
//};

struct VS_INPUT_SCENE
{
    float3 Position : ATTRIBUTE0; // vertex position
    float3 Normal : ATTRIBUTE1; // this normal comes in per-vertex
    float2 UV0 : ATTRIBUTE2;   // vertex texture coords
#ifdef HAS_TANGENT
    float4 Tangent: ATTRIBUTE3; // this normal comes in per-vertex
#endif
#ifdef HAS_WEIGHTS_0
    float4 JointsWeights0  : ATTRIBUTE4; //joints weights
    float4 JointsIndices0  : ATTRIBUTE5; // joints indices
#endif
};
//--------------------------------------------------------------------------------------
// mainVS
//--------------------------------------------------------------------------------------
VS_OUTPUT_SCENE gltfVertexFactory(VS_INPUT_SCENE input)
{
    VS_OUTPUT_SCENE Output;
#ifdef HAS_WEIGHTS_0
    matrix skinningMatrix;
    skinningMatrix  = GetCurrentSkinningMatrix(input.JointsWeights0, input.JointsIndices0);
#else
    matrix skinningMatrix =
    {
        { 1, 0, 0, 0 },
        { 0, 1, 0, 0 },
        { 0, 0, 1, 0 },
        { 0, 0, 0, 1 }
    };
#endif
    
    matrix transMatrix = mul(skinningMatrix, GetWorldMatrix());
    Output.Normal = normalize(mul(float4(input.Normal, 0), transMatrix).xyz);
#ifdef HASFUR
    float2 UVoffset = float2(0.2, 0.2) * FurOffset;
    UVoffset *= 0.1;
    Output.UV0 = input.UV0 * UVScale + UVoffset;
    Output.UV1 = input.UV0;
    float furLength_coeff = 1.0;
    float vGravityStength = 0.5;
	float3 Direction = lerp(input.Normal, Gravity * vGravityStength + input.Normal * (1.0 - vGravityStength), FurOffset);
	float3 P = input.Position + Direction * FurLength * FurOffset * furLength_coeff;
    Output.WorldPos = mul(float4(P, 1.0f),transMatrix).xyz;
    
	float SH = clamp(Output.Normal.y * 0.25 + 0.35, 0.0, 1.0);
    Output.SH = float3(SH, SH, SH );
#else
    Output.UV0 = input.UV0;
    Output.WorldPos = mul(float4(input.Position, 1.0f),transMatrix).xyz;
   
 #endif
    Output.svPosition = mul(float4(Output.WorldPos, 1.0f), GetCameraViewProj());
    
#ifdef HAS_WEIGHTS_0
        matrix prevSkinningMatrix;
        prevSkinningMatrix  = GetPreviousSkinningMatrix(input.Weights0, input.Joints0);
#else
    matrix prevSkinningMatrix =
    {
        { 1, 0, 0, 0 },
        { 0, 1, 0, 0 },
        { 0, 0, 1, 0 },
        { 0, 0, 0, 1 }
    };
 #endif
    
    Output.svCurrPosition = Output.svPosition; // current's frame vertex position 

    matrix prevTransMatrix = mul(prevSkinningMatrix,GetPrevWorldMatrix());
    float3 worldPrevPos = mul(float4(input.Position, 1),prevTransMatrix).xyz;
    Output.svPrevPosition = mul(float4(worldPrevPos, 1),GetPrevCameraViewProj());
    
    #ifdef HAS_TANGENT
        Output.Tangent = normalize(mul(float4(input.Tangent.xyz, 0),transMatrix).xyz);
        Output.Binormal = cross(Output.Normal, Output.Tangent) *input.Tangent.w;
    #endif
    return Output;
}