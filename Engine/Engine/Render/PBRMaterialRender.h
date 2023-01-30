#pragma once
#include "Engine/Render/MaterialRender.h"

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

		virtual void InitRenderResource(nlohmann::json& jsonObj);
		virtual void Draw(RenderCore::RHICommandContext& RHIContext, const MaterialRenderParam& RenderParam);
	private:
		void InitShader(const std::wstring& Path);
	private:
		std::shared_ptr< PBRMaterialRenderP> Impl;
	};
}