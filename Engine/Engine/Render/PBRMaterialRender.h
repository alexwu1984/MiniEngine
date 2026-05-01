#pragma once
#include "Engine/Render/MaterialRender.h"
#include "RHI/RHIShdader.h"

namespace Engine
{
	struct PBRMaterialRenderPrivate;
	class GltfMeshBuffer;
	class MaterialBase;

	class PBRMaterialRender : public MaterialRender
	{
	public:
		PBRMaterialRender(std::shared_ptr<GltfMeshBuffer> MeshBuffer,std::shared_ptr<MaterialBase> MeshMaterial);
		virtual ~PBRMaterialRender();

		virtual void InitRenderResource();
		virtual void SetBoneMatrix(const math::Matrix4x4& Mat, int32_t Index);
		virtual void Draw(RenderCore::RHICommandContext& RHIContext, const MaterialRenderParam& RenderParam) final;
		virtual void PreDraw(RenderCore::RHICommandContext& RHIContext, const MaterialRenderParam& RenderParam);
	protected:
		void DrawPrimitive(RenderCore::RHICommandContext& RHIContext);
		virtual void DrawMesh(RenderCore::RHICommandContext& RHIContext);
		virtual void PreDrawMesh(RenderCore::RHICommandContext& RHIContext);
		virtual bool IsNeedPreDraw() const;
		const MaterialRenderParam& GetRenderParam() const;
	protected:
		virtual void SetPipeLineState(RenderCore::RHICommandContext& RHIContext, std::shared_ptr<SceneTextures> TargetBuffer);

	private:
		virtual std::wstring GetShaderFileName() const;
		virtual void AddShaderMacro(std::vector<RenderCore::RHIShaderMacro> & ShaderMacros);
	private:
		void InitShader(const std::wstring& Path);
	private:
		PBRMaterialRenderPrivate* d_ptr;
	};
}