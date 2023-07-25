#pragma once
#include "core/inc.h"
#include "math/matrix4x4.h"
#include "math/vector2.h"
#include "RHI/RHIShaderDefine.h"


namespace Engine
{

	struct PerFrame
	{
		math::Matrix4x4     CameraCurrViewProj;
		math::Matrix4x4     CameraPrevViewProj;
		math::Matrix4x4     CameraCurrViewProjInverse;
		math::Vector4       CameraPos;
		float				IBLFactor;
		float				EmissiveFactor;
		math::Vector2       InvScreenResolution;

		math::Vector4       WireframeOptions;

		float				LodBias;
		math::Vector3       Padding;
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

	BEGIN_SHADER_STRUCT(CBPerFur, 4)
		DECLARE_PARAM(math::Vector3, Gravity)
		DECLARE_PARAM(float, FurOffset)
		DECLARE_PARAM(math::Vector3, FurColor)
		DECLARE_PARAM(float, FurLength)
		DECLARE_PARAM_VALUE(float, UVScale,1.0f)
		DECLARE_PARAM_VALUE(float, FurAmbientStrength,1.0f)
		DECLARE_PARAM_VALUE(float, FurLevel,1.0f)
		DECLARE_PARAM_VALUE(float, FurLightExposure,1.0f)
		BEGIN_STRUCT_CONSTRUCT(CBPerFur)
		END_STRUCT_CONSTRUCT
	END_SHADER_STRUCT
}