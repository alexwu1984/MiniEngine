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
		virtual void Draw(RHICommandContext& RHIContext);
	private:
		void InitShader(const std::wstring& Path);
	private:
		std::shared_ptr< PBRMaterialRenderP> Impl;
	};
}