#include "Engine/Render/MaterialRender.h"
#include "Engine/Render/MaterialRenderP.h"
#include "Engine/Thread/RenderThread.h"

namespace Engine
{

	MaterialRender::MaterialRender(std::shared_ptr<GltfMaterial> Material)
		:MaterialRenderData(std::make_shared<MaterialRenderP>())
	{
		MaterialRenderData->Material = Material;
	}

	MaterialRender::~MaterialRender()
	{
		if (GRenderThread)
		{
			GRenderThread->WaitForFinish();
		}
	}

	void MaterialRender::InitRenderResource(nlohmann::json& jsonObj)
	{

	}

}