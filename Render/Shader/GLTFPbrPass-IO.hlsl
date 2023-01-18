#ifndef GLTFPBRPassIO
#define GLTFPBRPassIO

//--------------------------------------------------------------------------------------
//  For PS input struct
//--------------------------------------------------------------------------------------

struct VS_OUTPUT_SCENE
{
    float4 svPosition   :    SV_POSITION;   // vertex position
    float3 WorldPos     :    WORLDPOS;      // vertex position
#ifdef HAS_NORMAL
    float3 Normal       :    NORMAL;        // this normal comes in per-vertex
#endif        
#ifdef HAS_TANGENT    
    float3 Tangent      :    TANGENT;       // this normal comes in per-vertex
    float3 Binormal     :    BINORMAL;     // this normal comes in per-vertex
#endif        
    float2 UV0       :    TEXCOORD0;    // vertex texture coords
};


#endif