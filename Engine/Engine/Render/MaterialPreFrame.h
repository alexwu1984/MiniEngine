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


}