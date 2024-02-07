// KHR_lights_punctual extension.
// see https://github.com/KhronosGroup/glTF/tree/master/extensions/2.0/Khronos/KHR_lights_punctual

#ifndef PerFrameStruct
#define PerFrameStruct

#define MAX_LIGHT_INSTANCES  80
#define MAX_SHADOW_INSTANCES 32

struct Light
{
    matrix        LightViewProj;
    matrix        LightView;

    float3        Direction;
    float         Range;

    float3        Color;
    float         Intensity;

    float3        Position;
    float         InnerConeCos;

    float         OuterConeCos;
    int           Type;
    float         DepthBias;
    int           ShadowMapIndex;
};

struct LightInstance
{
    matrix        LightViewProj;
    float3        Direction;
    float3        Position;
    int           ShadowMapIndex;
    float         DepthBias;
};

static const int LightType_Directional = 0;
static const int LightType_Point = 1;
static const int LightType_Spot = 2;

struct MaterialPerFrame
{
    float Metallic;
    int3 Padding;
};

struct PerFrame
{
    matrix        CameraCurrViewProj;
    matrix        CameraPrevViewProj;
    matrix        CameraCurrViewProjInverse;
    float4        CameraPos;
    float         IBLFactor;
    float         EmissiveFactor;
    float2        InvScreenResolution;
    float4        WireframeOptions;
    float         LodBias;
    float         IBLMIpCount;
    int           LightCount;
    int           Padding;
    float4        TemporalAAJitter;
    Light         Lights[MAX_LIGHT_INSTANCES];
    MaterialPerFrame Material;
};

cbuffer cbPerFrame : register(b0)
{
    PerFrame myPerFrame;
};

cbuffer cbPerObject : register(b1)
{
    matrix myPerObject_u_mCurrWorld;
    matrix myPerObject_u_mPrevWorld;
}

cbuffer cbPerFur : register(b3)
{
    float3 Gravity;
    float FurOffset;
    float3 FurColor;
    float FurLength;
    float UVScale;
    float FurAmbientStrength;
    float FurLevel;
    float FurLightExposure;
    uint3 DrawSolid;
}

matrix GetWorldMatrix()
{
    return myPerObject_u_mCurrWorld;
}

matrix GetCameraViewProj()
{
    return myPerFrame.CameraCurrViewProj;
}

matrix GetPrevWorldMatrix()
{
    return myPerObject_u_mPrevWorld;
}

matrix GetPrevCameraViewProj()
{
    return myPerFrame.CameraPrevViewProj;
}

#endif