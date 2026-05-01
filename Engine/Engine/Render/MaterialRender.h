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
	class SceneTextures;

	struct MaterialRenderParam
	{
		math::Matrix4x4 CurrModelMatrix;
		math::Matrix4x4 PrevModelMatrix;
		math::Matrix4x4 CurrViewProjMatrix;
		math::Matrix4x4 PrevViewProjMatrix;
		math::Matrix4x4 CurrViewProjInverseMatrix;
		math::Matrix4x4 PrevViewProjInverseMatrix;
		math::Matrix4x4 RotateIBL;
		math::Vector4 CameraPos;
		math::Vector4 TemporalAAJitter;
		bool HasSkin = false;
		std::weak_ptr<PreProcessor> preProcessor;
		std::vector< Light> lightInfos;
		std::shared_ptr<SceneTextures> TargetBuffer;
		bool bUnlit = false;
		/** Per-view skylight IBL scale (0 = off). */
		float SkyLightIBLScale = 0.f;
	};

	class SceneModelAsset;

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