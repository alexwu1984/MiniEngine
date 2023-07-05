#include "Engine/Render/FurMaterialRender.h"
#include "RHI/RHIShdader.h"
#include "Engine.h"

namespace Engine
{

	FurMaterialRender::FurMaterialRender(std::shared_ptr<GltfMeshBuffer> MeshBuffer, std::shared_ptr< GltfMaterial> MeshMaterial)
		:PBRMaterialRender(MeshBuffer,MeshMaterial)
	{

	}

	FurMaterialRender::~FurMaterialRender()
	{

	}

	void FurMaterialRender::InitRenderResource(std::shared_ptr< GltfModelConfig> ModelConfig)
	{
		PBRMaterialRender::InitRenderResource(ModelConfig);
	}

}
