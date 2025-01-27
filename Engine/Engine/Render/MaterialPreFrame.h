#pragma once
#include "core/inc.h"
#include "math/matrix4x4.h"
#include "math/vector2.h"
#include "RHI/RHIShaderDefine.h"


namespace Engine
{
#define MAX_LIGHT_INSTANCES  80
#define MAX_SHADOW_INSTANCES 32

	static const int LightType_Directional = 0;
	static const int LightType_Point = 1;
	static const int LightType_Spot = 2;

	struct Light
	{
		math::Matrix4x4	LightViewProj;
		math::Matrix4x4 LightView;

		math::Vector3	Direction;
		float			Range{ 0.f };

		math::Vector3   Color;
		float			Intensity{ 1.f };

		math::Vector3   Position;
		float			InnerConeCos{ 0.f };

		float			OuterConeCos{ 0.f };
		int				Type{ 0 };
		float			DepthBias{ 0.f };
		int				ShadowMapIndex{ -1 };
	};

	struct MaterialPerFrame
	{
		float Metallic{ 1.f };
		int padding0{ 0 };
		int padding1{ 0 };
		int padding2{ 0 };
	};

	struct PerFrame
	{
		math::Matrix4x4     CameraCurrViewProj;
		math::Matrix4x4     CameraPrevViewProj;
		math::Matrix4x4     CameraCurrViewProjInverse;
		math::Matrix4x4     RotateIBL;
		math::Vector4       CameraPos;
		float				IBLFactor{ 1.f };
		float				EmissiveFactor{ 100.f };
		math::Vector2       InvScreenResolution;
		math::Vector4       WireframeOptions;
		float				LodBias{ 0.f };
		float				IBLMIpCount{ 1.f };
		int32_t				LightCount{ 0 };
		int32_t				Padding;
		math::Vector4		TemporalAAJitter{1.f, 1.f, 1.f, 1.f};
		Light				Lights[MAX_LIGHT_INSTANCES];
		MaterialPerFrame	Material;
	};

	BEGIN_SHADER_STRUCT(CBPerFrame, 0)
		DECLARE_PARAM(PerFrame, myPerFrame)	
		BEGIN_STRUCT_CONSTRUCT(CBPerFrame)
		END_STRUCT_CONSTRUCT
	END_SHADER_STRUCT

	BEGIN_SHADER_STRUCT(CBPerObject, 1)
		DECLARE_PARAM(math::Matrix4x4, myPerObject_u_mCurrWorld)
		DECLARE_PARAM(math::Matrix4x4, myPerObject_u_mPrevWorld)
		BEGIN_STRUCT_CONSTRUCT(CBPerObject)
		END_STRUCT_CONSTRUCT
	END_SHADER_STRUCT

	struct Matrix2
	{
		math::Matrix4x4 Current;
		math::Matrix4x4 Previous;
	};

	BEGIN_SHADER_STRUCT(CBPerSkeleton, 2)
		DECLARE_ARRAY_PARAM(Matrix2,200, PerSkeleton_u_ModelMatrix)
		BEGIN_STRUCT_CONSTRUCT(CBPerSkeleton)
		END_STRUCT_CONSTRUCT
	END_SHADER_STRUCT

	BEGIN_SHADER_STRUCT(CBPerFur, 3)
		DECLARE_PARAM(math::Vector3, Gravity)
	DECLARE_PARAM(float, FurOffset)
	DECLARE_PARAM(math::Vector3, FurColor)
	DECLARE_PARAM(float, FurLength)
	DECLARE_PARAM_VALUE(float, UVScale, 1.0f)
	DECLARE_PARAM_VALUE(float, FurAmbientStrength, 1.0f)
	DECLARE_PARAM_VALUE(float, FurLevel, 1.0f)
	DECLARE_PARAM_VALUE(float, FurLightExposure, 1.0f)
	DECLARE_PARAM_VALUE(uint32_t, DrawSolid, 0)
	DECLARE_PARAM(uint32_t, FurPad1)
	DECLARE_PARAM(uint32_t, FurPad2)
	DECLARE_PARAM(uint32_t, FurPad3)
	BEGIN_STRUCT_CONSTRUCT(CBPerFur)
		END_STRUCT_CONSTRUCT
	END_SHADER_STRUCT

}