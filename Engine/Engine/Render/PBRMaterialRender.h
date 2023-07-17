#pragma once
#include "Engine/Render/MaterialRender.h"
#include "RHI/RHIShdader.h"

namespace Engine
{
	struct PBRMaterialRenderP;
	class GltfMeshBuffer;
	class GltfMaterial;

	class PBRMaterialRender : public MaterialRender
	{
	public:
		PBRMaterialRender(std::shared_ptr<GltfMeshBuffer> MeshBuffer,std::shared_ptr< GltfMaterial> MeshMaterial);
		virtual ~PBRMaterialRender();

		virtual void InitRenderResource();
		virtual void SetBoneMatrix(const math::Matrix4x4& Mat, int32_t Index);
		virtual void Draw(RenderCore::RHICommandContext& RHIContext, const MaterialRenderParam& RenderParam) final;
	protected:
		void DrawPrimitive(RenderCore::RHICommandContext& RHIContext);
		virtual void DrawMesh(RenderCore::RHICommandContext& RHIContext);
	private:
		virtual std::wstring GetShaderFileName() const;
		virtual void AddShaderMacro(std::vector<RenderCore::RHIShaderMacro> & ShaderMacros);
	private:
		void InitShader(const std::wstring& Path);
	private:
		std::shared_ptr< PBRMaterialRenderP> Impl;
	};
}