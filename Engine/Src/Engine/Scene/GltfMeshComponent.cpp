#include "Scene/GltfMeshComponent.h"
#include "GltfModel/GltfModel.h"
#include "Render/PBRMaterialRender.h"
#include "GltfModel/GltfMesh.h"

namespace Engine
{
	struct GltfMeshComponentP
	{
		GltfModel Model;
		std::vector<std::shared_ptr<MaterialRender>> Renders;
	};

	GltfMeshComponent::GltfMeshComponent(std::weak_ptr<Actor> Owner)
		:Component(Owner)
		, Impl(std::make_shared<GltfMeshComponentP>())
	{
		
	}

	GltfMeshComponent::~GltfMeshComponent()
	{

	}

	//Todo: load json config
	bool GltfMeshComponent::Load(const std::wstring& FileName)
	{
		if (!Impl->Model.Load(FileName))
		{
			return false;
		}

		const auto& Meshs = Impl->Model.GetModelMesh();
		for (const auto& Mesh: Meshs)
		{
			//default Material
			std::shared_ptr<PBRMaterialRender> PBRMaterial = std::make_shared<PBRMaterialRender>(Mesh);
			Impl->Renders.emplace_back(PBRMaterial);
		}

		return true;
	}

	void GltfMeshComponent::Draw(RHICommandContext& RHIContext, std::shared_ptr<CameraComponent> Camera)
	{

	}

}
