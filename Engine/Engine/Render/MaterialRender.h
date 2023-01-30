#pragma once
#include "core/inc.h"
#include "tinygltf/json.h"
#include "math/matrix4x4.h"

namespace RenderCore
{
	class RHICommandContext;
}

namespace Engine
{
	
	struct MaterialRenderParam
	{
		math::Matrix4x4 CurrModelMatrix;
		math::Matrix4x4 PrevModelMatrix;
		math::Matrix4x4 CurrViewProjMatrix;
		math::Matrix4x4 CurrViewProjInverseMatrix;
		math::Matrix4x4 PrevViewProjMatrix;
		math::Vector4 CameraPos;
	};

	class MaterialRender
	{
	public:
		MaterialRender() = default;
		virtual ~MaterialRender();
		
		virtual void InitRenderResource(nlohmann::json& jsonObj) = 0;
		virtual void Draw(RenderCore::RHICommandContext& RHIContext,const MaterialRenderParam& RenderParam) = 0;
	};
}