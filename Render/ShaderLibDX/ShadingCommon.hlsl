// Copyright Epic Games, Inc. All Rights Reserved.
//
// Engine/Shaders/Private/ShadingCommon.ush

#ifndef SHADINGCOMMON_HLSL
#define SHADINGCOMMON_HLSL

// SHADINGMODELID_* occupy the 4 low bits of an 8bit channel and SKIP_* occupy the 4 high bits
#define SHADINGMODELID_UNLIT				0
#define SHADINGMODELID_DEFAULT_LIT			1
#define SHADINGMODELID_SUBSURFACE			2
#define SHADINGMODELID_PREINTEGRATED_SKIN	3
#define SHADINGMODELID_CLEAR_COAT			4
#define SHADINGMODELID_SUBSURFACE_PROFILE	5
#define SHADINGMODELID_TWOSIDED_FOLIAGE		6
#define SHADINGMODELID_HAIR					7
#define SHADINGMODELID_CLOTH				8
#define SHADINGMODELID_EYE					9
#define SHADINGMODELID_SINGLELAYERWATER		10
#define SHADINGMODELID_THIN_TRANSLUCENT		11
#define SHADINGMODELID_NUM					12
#define SHADINGMODELID_MASK					0xF		// 4 bits reserved for ShadingModelID

// The flags are defined so that 0 value has no effect!
// These occupy the 4 high bits in the same channel as the SHADINGMODELID_*
#define HAS_ANISOTROPY_MASK				(1 << 4)
#define SKIP_PRECSHADOW_MASK			(1 << 5)
#define ZERO_PRECSHADOW_MASK			(1 << 6)
#define SKIP_VELOCITY_MASK				(1 << 7)

// Hair reflectance component (R, TT, TRT, Local Scattering, Global Scattering, Multi Scattering,...)
#define HAIR_COMPONENT_R			0x1u
#define HAIR_COMPONENT_TT			0x2u
#define HAIR_COMPONENT_TRT			0x4u
#define HAIR_COMPONENT_LS			0x8u
#define HAIR_COMPONENT_GS			0x10u
#define HAIR_COMPONENT_MULTISCATTER	0x20u
#define HAIR_COMPONENT_TT_MODEL  	0x40u

#endif // SHADINGCOMMON_HLSL
