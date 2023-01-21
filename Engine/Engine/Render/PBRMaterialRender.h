#pragma once
#include "Engine/Render/MaterialRender.h"

namespace Engine
{
	struct PBRMaterialRenderP;
	class GltfMesh;

	class PBRMaterialRender : public MaterialRender
	{
	public:
		PBRMaterialRender(std::shared_ptr<GltfMesh> Mesh);
		virtual ~PBRMaterialRender();

		virtual void InitRenderResource(nlohmann::json& jsonObj);
		virtual void InitShader(const std::wstring& Path);
		virtual void Draw(RHICommandContext& RHIContext) {};
	private:
		std::shared_ptr< PBRMaterialRenderP> Impl;
	};
}