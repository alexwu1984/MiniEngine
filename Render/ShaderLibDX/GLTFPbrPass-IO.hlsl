#ifndef GLTFPBRPassIO
#define GLTFPBRPassIO

//--------------------------------------------------------------------------------------
//  For PS input struct
//--------------------------------------------------------------------------------------

struct VS_OUTPUT_SCENE
{
    float4 svPosition   :    SV_POSITION;   // vertex position
    float3 WorldPos     :    POSITION0;      // vertex position
    float4 LightPos     :    POSITION1;
    float3 Normal       :    NORMAL;        // this normal comes in per-vertex 
    float3 SH :              NORMAL1;
#ifdef HAS_TANGENT    
    float3 Tangent      :    TANGENT;       // this normal comes in per-vertex
    float3 Binormal     :    BINORMAL;     // this normal comes in per-vertex
#endif        
    float2 UV0          :    TEXCOORD0;    // vertex texture coords
    float2 UV1          :    TEXCOORD1; // vertex texture coords
    float4 svCurrPosition :  TEXCOORD2; // current's frame vertex position 
    float4 svPrevPosition :  TEXCOORD3; // previous' frame vertex position
#ifdef HASFUR
    /** Shell layer t in (0,1]; PS must match VS (instanced shells or cb FurOffset). */
    nointerpolation float FurShellOffset : TEXCOORD6;
#endif
};


#endif