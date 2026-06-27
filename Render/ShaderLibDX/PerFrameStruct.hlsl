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

static const int LightType_Directional = 0;
static const int LightType_Point = 1;
static const int LightType_Spot = 2;

struct MaterialPerFrame
{
    float Metallic;
    float AlphaCutoff;
    float TransmissionFactor;
    float AttenuationDistance;
    float3 AttenuationColor;
    float ThicknessFactor;
    float MaterialIor;
    float MaterialDispersion;
    uint AlphaMask;
    uint MaterialShaderFlags;
};

static const uint kMatShaderFlag_WriteBaseColorAlpha = 1u << 0;
static const uint kMatShaderFlag_DoubleSidedShading = 1u << 1;
static const uint kMatShaderFlag_ShadowAlphaClip = 1u << 2;
static const uint kMatShaderFlag_Transmission = 1u << 3;

struct PerFrame
{
    // View / camera matrices
    matrix        CameraCurrViewProj;
    matrix        CameraPrevViewProj;
    matrix        CameraCurrViewProjInverse;
    /** World → view (row-vector: mul(float4(world,1), M).xyz); matches ClusterLightBuildCS ClusterViewMatrix / FSceneViewData::ViewMatrix. */
    row_major matrix CameraWorldToView;
    matrix        RotateIBL;

    // View / camera parameters
    float4        CameraPos;
    float2        InvScreenResolution;
    float         CameraNearZ;
    float         CameraFarZ;

    // IBL / material globals
    float         IBLFactor;
    float         EmissiveFactor;
    float         LodBias;
    float         IBLMIpCount;

    float         GroundIBLIntensity;
    float         HemiIBLBlendPower;
    int           SplitHemisphereIBL;
    int           _PadSplitHemiIBL;
    /** .xy = optional IBL × dir shadow vis; .z = pow(bakedAO, z) on diffuse IBL only; UE default: xy=0, z=1. */
    float4        IBLDirShadowCoupling;

    // Debug / view flags
    float4        WireframeOptions;
    int           LightCount;
    /** First directional in view light list (shadow map / main dir); -1 if none. */
    int           PrimaryDirectionalLightIndex;
    int           bUnlit;
    int           _PadPerFrameLightHeader;
    float4        TemporalAAJitter;

    Light         Lights[MAX_LIGHT_INSTANCES];

    /** Trailing pad: keep sizeof(cbPerFrame) % 16 == 0. */
    uint4         _PerFramePadAfterLights;
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

matrix GetCameraViewProjInverse()
{
    return myPerFrame.CameraCurrViewProjInverse;
}

/** Skylight cubemap capture: reconstruct world-space view ray from perspective-interpolated clip coordinates. */
float3 SkyCubeDirectionFromHClip(float4 HClip)
{
    float4 worldH = mul(HClip, myPerFrame.CameraCurrViewProjInverse);
    return normalize(worldH.xyz / worldH.w);
}

matrix GetPrevWorldMatrix()
{
    return myPerObject_u_mPrevWorld;
}

matrix GetPrevCameraViewProj()
{
    return myPerFrame.CameraPrevViewProj;
}

/** True when the primary directional slot uses the 2D directional shadow map (t8). Spot/point use other paths. */
bool IsEnableShadow()
{
    int idx = myPerFrame.PrimaryDirectionalLightIndex;
    return idx >= 0 && idx < MAX_LIGHT_INSTANCES
        && myPerFrame.Lights[idx].Type == LightType_Directional && myPerFrame.Lights[idx].ShadowMapIndex >= 0;
}

/** Slot into Lights[] for main directional; 0 when index invalid (matches legacy fallback to Lights[0]). */
uint GetMainDirectionalLightSlot()
{
    const int i = myPerFrame.PrimaryDirectionalLightIndex;
    uint outSlot = 0u;
    if (i >= 0 && i < MAX_LIGHT_INSTANCES)
        outSlot = (uint)i;
    return outSlot;
}

matrix GetMainLightViewProj()
{
    const uint slot = GetMainDirectionalLightSlot();
    return myPerFrame.Lights[slot].LightViewProj;
}

Light GetMainLight()
{
    const uint slot = GetMainDirectionalLightSlot();
    return myPerFrame.Lights[slot];
}

#endif