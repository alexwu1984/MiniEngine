#pragma once
#include "Engine/Render/MaterialRender.h"
#include "RHI/RHIShdader.h"

namespace Engine
{
	struct PBRMaterialRenderPrivate;
	class GltfMeshBuffer;
	class MaterialBase;
	class DeferredLightingPass;
	class FWorldSceneRender;
	struct FSceneViewData;

	class PBRMaterialRender : public MaterialRender
	{
	public:
		PBRMaterialRender(std::shared_ptr<GltfMeshBuffer> MeshBuffer,std::shared_ptr<MaterialBase> MeshMaterial);
		virtual ~PBRMaterialRender();

		virtual void InitRenderResource();
		virtual void SetBoneMatrix(const math::Matrix4x4& Mat, int32_t Index);
		void ResetSkeletonPaletteIdentity() override;
		void OnSkinnedPaletteUploaded(int32_t NumBones) override;
		virtual void Draw(RenderCore::RHICommandContext& RHIContext, const MaterialRenderParam& RenderParam) final;
		virtual void PreDraw(RenderCore::RHICommandContext& RHIContext, const MaterialRenderParam& RenderParam);
	protected:
		/** D3D12: ps_5_1 Texture2D[] for first material SRVs (PBR: t0–t4; FurMaterial: t0–t1 albedo+noise). */
		virtual bool WantsRHIBindless() const { return true; }
		/** Fur uses a different VS/PS pair; skip compiling TranslucentPBRForward.hlsl for fur instances. */
		virtual bool ShouldCompileTranslucentForwardPixelShader() const { return true; }
		void StoreRenderParam(const MaterialRenderParam& RenderParam);
		void BindDrawUniformBuffers(RenderCore::RHICommandContext& RHIContext);

		void DrawPrimitive(RenderCore::RHICommandContext& RHIContext);
		virtual void DrawMesh(RenderCore::RHICommandContext& RHIContext);
		virtual void PreDrawMesh(RenderCore::RHICommandContext& RHIContext);
		virtual bool IsNeedPreDraw() const;
		const MaterialRenderParam& GetRenderParam() const;
		std::shared_ptr<GltfMeshBuffer> GetPBRMeshBuffer() const;
		std::shared_ptr<RenderCore::RHIVertexShader> GetPBRVertexShader() const;
		std::shared_ptr<RenderCore::RHIPixelShader> GetPBRPixelShader() const;
		std::shared_ptr<MaterialBase> GetPBRMeshMaterial() const;
	protected:
		virtual void SetPipeLineState(RenderCore::RHICommandContext& RHIContext, std::shared_ptr<FSceneTextures> SceneTextures);

		void RefreshIBLMipAndRebindPerFrame(RenderCore::RHICommandContext& RHIContext, const MaterialRenderParam& RenderParam);
		void BindDeferredBaseMaterialTextures(RenderCore::RHICommandContext& RHIContext);

	public:
		void BeginDeferredOpaqueDrawBatch(RenderCore::RHICommandContext& RHIContext, const MaterialRenderParam& RenderParam);
		void DrawDeferredOpaqueBatchInstance(RenderCore::RHICommandContext& RHIContext, const MaterialRenderParam& RenderParam);

		/** After deferred lighting: blend into SceneColor only; depth test, no depth write (see TranslucentPBRForward.hlsl). */
		void DrawTranslucentForwardLit(RenderCore::RHICommandContext& RHIContext, const MaterialRenderParam& RenderParam, DeferredLightingPass* DeferredLighting,
									   FWorldSceneRender* WorldSceneRender, const std::shared_ptr<const FSceneViewData>& ViewData);

	private:
		virtual std::wstring GetShaderFileName() const;
		virtual void AddShaderMacro(std::vector<RenderCore::RHIShaderMacro> & ShaderMacros);
	private:
		void InitShader(const std::wstring& Path);
	private:
		PBRMaterialRenderPrivate* d_ptr;
	};
}