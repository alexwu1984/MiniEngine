#include "Scene/GltfMeshComponent.h"
#include "GltfModel/GltfModel.h"
#include "GltfModel/GltfMesh.h"
#include "GltfModel/GltfMaterial.h"
#include "GltfModel/GltfSkeleton.h"
#include "GltfModel/GltfModelConfig.h"
#include "Render/PBRMaterialRender.h"
#include "Render/FurMaterialRender.h"
#include "RHI/RHICommandContext.h"
#include "RHI/DynamicRHI.h"
#include "Scene/Actor.h"
#include "Scene/CameraComponent.h"
#include "Thread/RenderThread.h"
#include "core/logger.h"
#include "Engine/Engine.h"
#include "Scene/SceneView.h"
#include "Render/SceneRender.h"

namespace Engine
{
	IMP_COMPONENT_CLASS_NAME(GltfMeshComponent)
	IMP_COMPONENT_TRAITS_CLASS_NAME(GltfMeshComponent)

	struct GltfMeshComponentP
	{
		GltfModel Model;
		float TotalDeltaTime = 0.f;
		std::shared_ptr< GltfModelConfig>  ModelConfig;
	};

	GltfMeshComponent::GltfMeshComponent(std::weak_ptr<Actor> Owner)
		:Component(Owner)
		, Impl(std::make_shared<GltfMeshComponentP>())
	{
		
	}

	GltfMeshComponent::~GltfMeshComponent()
	{
		if (GRenderThread)
		{
			GRenderThread->WaitForFinish();
		}
	}

	//Todo: load json config
	bool GltfMeshComponent::Load(const std::wstring& FileName)
	{
		std::filesystem::path Path = FileName;
		if (!Path.has_extension())
		{
			core::err() << __FUNCTION__ << " Load File failed:" << FileName;
			return false;
		}
		std::wstring Extension =  Path.extension().wstring();

		if (Extension == L".json")
		{
			Impl->ModelConfig = std::make_shared<GltfModelConfig>(std::static_pointer_cast<GltfMeshComponent>(this->shared_from_this()));
			if (Impl->ModelConfig->Load(FileName))
			{
				std::wstring Path = std::filesystem::path(FileName).parent_path().wstring();
				Path += L"/" + Impl->ModelConfig->GetModel();
				if (!Impl->Model.Load(Path, Impl->ModelConfig))
				{
					return false;
				}
			}
			else
			{
				return false;
			}
		}
		else
		{
			if (!Impl->Model.Load(FileName,nullptr))
			{
				return false;
			}
		}


		return true;
	}

	GltfModel& GltfMeshComponent::GetModel() const
	{
		return Impl->Model;
	}

	math::AABB3 GltfMeshComponent::GetModelBox() const
	{
		return Impl->Model.GetModelBox();
	}


	void GltfMeshComponent::Tick(float deltaTime)
	{
		Impl->TotalDeltaTime += deltaTime / 5.f;
		Impl->Model.Play(Impl->TotalDeltaTime, deltaTime);
	}

	void GltfMeshComponent::OnUpdateWorldTransform(float deltaTime)
	{
		auto& RootNodes = Impl->Model.GetSkeleton()->GetRootNode();
		if (!RootNodes.empty())
		{
			math::Matrix4x4 WorldTransform = GetOwner()->GetWorldTransform();
			RootNodes[0]->ParentMat = WorldTransform;
		}
	}

	bool GltfMeshComponent::GatherMesh(std::vector<std::shared_ptr<GltfMesh>>& Meshes, math::Matrix4x4& WorldTransform, std::shared_ptr<CameraComponent> Camera)
	{
		WorldTransform = GetOwner()->GetWorldTransform();
		math::AABB3 Box = Impl->Model.GetModelBox().Transform(WorldTransform);
		bool Render = Camera->GetFrustum().Intersects(Box);
		if (Render)
		{
			auto& TmpMeshs = Impl->Model.GetModelMesh();
			std::for_each(TmpMeshs.begin(), TmpMeshs.end(), [&Meshes](std::shared_ptr<GltfMesh> Item) {
				Meshes.push_back(Item);
				});
			
		}
		return Render;
	}

}
