#pragma once
#include "Engine/Render/PBRMaterialRender.h"

namespace Engine
{
	struct FurMaterialRenderPrivate;

	class FurMaterialRender : public PBRMaterialRender
	{
	public:
		FurMaterialRender(std::shared_ptr<GltfMeshBuffer> MeshBuffer, std::shared_ptr< GltfMaterial> MeshMaterial);
		virtual ~FurMaterialRender();

		virtual void InitRenderResource() override;
		
	private:
		virtual std::wstring GetShaderFileName() const;
		virtual void AddShaderMacro(std::vector<RenderCore::RHIShaderMacro>& ShaderMacros);
		virtual void DrawMesh(RenderCore::RHICommandContext& RHIContext) override;
		virtual void PreDrawMesh(RenderCore::RHICommandContext& RHIContext) override;
		virtual bool IsNeedPreDraw() const override;
	private:
		FurMaterialRenderPrivate* d_ptr = nullptr;
	};
}