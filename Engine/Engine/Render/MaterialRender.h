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
	class PreProcessor;

	enum class AABBPosition
	{
		Near,
		Far
	};

	struct MaterialRenderParam
	{
		math::Matrix4x4 CurrModelMatrix;
		math::Matrix4x4 PrevModelMatrix;
		math::Matrix4x4 CurrViewProjMatrix;
		math::Matrix4x4 CurrViewProjInverseMatrix;
		math::Matrix4x4 PrevViewProjMatrix;
		math::Vector4 CameraPos;
		AABBPosition PosType;
		bool HasSkin = false;
		std::weak_ptr<PreProcessor> _PreProcessor;
	};

	class GltfModelConfig;

	class MaterialRender
	{
	public:
		MaterialRender() = default;
		virtual ~MaterialRender();
		
		virtual void InitRenderResource() = 0;
		virtual void SetBoneMatrix(const math::Matrix4x4& Mat, int32_t Index) = 0;
		virtual void Draw(RenderCore::RHICommandContext& RHIContext,const MaterialRenderParam& RenderParam) = 0;
		virtual void PreDraw(RenderCore::RHICommandContext& RHIContext, const MaterialRenderParam& RenderParam) = 0;
	};
}