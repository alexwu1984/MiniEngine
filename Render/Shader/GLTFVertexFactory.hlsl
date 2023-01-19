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
#endif
#else
    matrix skinningMatrix =
    {
        { 1, 0, 0, 0 },
        { 0, 1, 0, 0 },
        { 0, 0, 1, 0 },
        { 0, 0, 0, 1 }
    };
#endif
    matrix transMatrix = mul(GetWorldMatrix(), skinningMatrix);
    Output.WorldPos = mul(transMatrix, float4(input.Position, 1)).xyz;
    Output.svPosition = mul(GetCameraViewProj(), float4(Output.WorldPos, 1));

    Output.Normal  = normalize(mul(transMatrix, float4(input.Normal, 0)).xyz);
#ifdef HAS_TANGENT
    Output.Tangent = normalize(mul(transMatrix, float4(input.Tangent.xyz, 0)).xyz);
    Output.Binormal = cross(Output.Normal, Output.Tangent) *input.Tangent.w;
#endif
    Output.UV0 = input.UV0;

    return Output;
}