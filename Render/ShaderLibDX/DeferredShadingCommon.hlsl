// Copyright Epic Games, Inc. All Rights Reserved.
//
// Engine/Shaders/Private/DeferredShadingCommon.ush (subset + MaterialAux packing)

#ifndef DEFERREDSHADINGCOMMON_HLSL
#define DEFERREDSHADINGCOMMON_HLSL

#include "ShadingCommon.hlsl"

float EncodeShadingModelIdAndSelectiveOutputMask(uint ShadingModelId, uint SelectiveOutputMask)
{
	uint Value = (ShadingModelId & SHADINGMODELID_MASK) | SelectiveOutputMask;
	return (float)Value / (float)0xFF;
}

uint DecodeShadingModelId(float InPackedChannel)
{
	return ((uint)round(InPackedChannel * (float)0xFF)) & SHADINGMODELID_MASK;
}

uint DecodeSelectiveOutputMask(float InPackedChannel)
{
	return ((uint)round(InPackedChannel * (float)0xFF)) & ~SHADINGMODELID_MASK;
}

bool IsHairShadingModel(uint ShadingModelID)
{
	return ShadingModelID == SHADINGMODELID_HAIR;
}

float4 EncodeMaterialAux_DefaultLit()
{
	return float4(EncodeShadingModelIdAndSelectiveOutputMask(SHADINGMODELID_DEFAULT_LIT, 0), 0.0, 0.0, 0.0);
}

float4 EncodeMaterialAux_Hair(float HairAmbientStrength, float HairLightExposure, float HairCoverageAlpha)
{
	return float4(EncodeShadingModelIdAndSelectiveOutputMask(SHADINGMODELID_HAIR, 0), HairAmbientStrength, HairLightExposure, HairCoverageAlpha);
}

#endif // DEFERREDSHADINGCOMMON_HLSL
