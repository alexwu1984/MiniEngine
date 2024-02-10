#pragma once
#include "Render/MaterialPreFrame.h"
#include "tinygltf/json.h"

namespace RenderCore
{
	class RHICommandContext;
}

namespace Engine
{
	class PreProcessor;

	struct MaterialRenderParam
	{
		math::Matrix4x4 CurrModelMatrix;
		math::Matrix4x4 PrevModelMatrix;
		math::Matrix4x4 CurrViewProjMatrix;
		math::Matrix4x4 PrevViewProjMatrix;
		math::Matrix4x4 CurrViewProjInverseMatrix;
		math::Matrix4x4 PrevViewProjInverseMatrix;
		math::Vector4 CameraPos;
		math::Vector4 TemporalAAJitter;
		bool HasSkin = false;
		std::weak_ptr<PreProcessor> preProcessor;
		std::vector< Light> lightInfos;
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