#pragma once
#include "Engine/Render/PBRMaterialRender.h"

namespace Engine
{
	class FurMaterialRender : public PBRMaterialRender
	{
	public:
		FurMaterialRender(std::shared_ptr<GltfMeshBuffer> MeshBuffer, std::shared_ptr< GltfMaterial> MeshMaterial);
		virtual ~FurMaterialRender();

		virtual void InitRenderResource(std::shared_ptr< GltfModelConfig> ModelConfig) override;
	};
}