#pragma once
#include "Engine/Render/PBRMaterialRender.h"
#include "Scene/SceneModelSettings.h"

namespace Engine
{
	struct FurMaterialRenderPrivate;
	class DeferredLightingPass;
	class FWorldSceneRender;
	struct FSceneViewData;

	class FurMaterialRender : public PBRMaterialRender
	{
	public:
		FurMaterialRender(std::shared_ptr<GltfMeshBuffer> MeshBuffer, std::shared_ptr< MaterialBase> MeshMaterial,
						  const FurConfig& InConifg,
						  std::shared_ptr<RenderCore::RHITexture2D> NoiseTex);
		virtual ~FurMaterialRender();

		virtual void InitRenderResource() override;
		void PreDraw(RenderCore::RHICommandContext& RHIContext, const MaterialRenderParam& RenderParam) override;

		/** After deferred lighting: binds IBL/shadow SRVs for forward PS (after PSO) when DeferredLighting+view+world are non-null. */
		void DrawForwardFur(RenderCore::RHICommandContext& RHIContext, const MaterialRenderParam& RenderParam, DeferredLightingPass* FurSharedBind = nullptr,
							FWorldSceneRender* WorldSceneRender = nullptr, const std::shared_ptr<const FSceneViewData>& ViewData = {});

	protected:
		virtual void SetPipeLineState(RenderCore::RHICommandContext& RHIContext, std::shared_ptr<SceneTextures> TargetBuffer) override;

	private:
		void DrawDeferredInnerBase(RenderCore::RHICommandContext& RHIContext, const MaterialRenderParam& RenderParam);
		virtual std::wstring GetShaderFileName() const;
		virtual void AddShaderMacro(std::vector<RenderCore::RHIShaderMacro>& ShaderMacros);
		virtual void DrawMesh(RenderCore::RHICommandContext& RHIContext) override;
		virtual void PreDrawMesh(RenderCore::RHICommandContext& RHIContext) override;
		virtual bool IsNeedPreDraw() const override;
	private:
		FurMaterialRenderPrivate* d_ptr = nullptr;
	};
}