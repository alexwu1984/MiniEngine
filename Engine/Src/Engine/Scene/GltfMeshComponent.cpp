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

	struct GltfMeshComponentPrivate
	{
		GltfModel Model;
		float TotalDeltaTime = 0.f;
		std::shared_ptr< GltfModelConfig>  ModelConfig;
	};

	GltfMeshComponent::GltfMeshComponent(std::weak_ptr<Actor> Owner)
		:Component(Owner)
		, d_ptr(new GltfMeshComponentPrivate())
	{
		
	}

	GltfMeshComponent::~GltfMeshComponent()
	{
		if (GRenderThread)
		{
			GRenderThread->WaitForFinish();
		}
		delete d_ptr;
	}

	//Todo: load json config
	bool GltfMeshComponent::Load(const std::wstring& FileName)
	{
		C_P(GltfMeshComponent);
		std::filesystem::path Path = FileName;
		if (!Path.has_extension())
		{
			core::err() << __FUNCTION__ << " Load File failed:" << FileName;
			return false;
		}
		std::wstring Extension =  Path.extension().wstring();

		if (Extension == L".json")
		{
			d->ModelConfig = std::make_shared<GltfModelConfig>(std::static_pointer_cast<GltfMeshComponent>(this->shared_from_this()));
			if (d->ModelConfig->Load(FileName))
			{
				std::wstring Path = std::filesystem::path(FileName).parent_path().wstring();
				Path += L"/" + d->ModelConfig->GetModelName();
				if (!d->Model.Load(Path, d->ModelConfig))
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
			if (!d->Model.Load(FileName,nullptr))
			{
				return false;
			}
		}

		return true;
	}

	bool GltfMeshComponent::Load(const nlohmann::json& GltfJson)
	{
		C_P(GltfMeshComponent);
		d->ModelConfig = std::make_shared<GltfModelConfig>(std::static_pointer_cast<GltfMeshComponent>(this->shared_from_this()));
		if (d->ModelConfig->Load(GltfJson))
		{
			std::wstring Path = GEngine->GetModelPath();
			Path += L"/" + d->ModelConfig->GetModelName();
			if (!d->Model.Load(Path, d->ModelConfig))
			{
				return false;
			}
			return true;
		}
		else
		{
			return false;
		}
	}

	GltfModel& GltfMeshComponent::GetModel() const
	{
		C_P(GltfMeshComponent);
		return d->Model;
	}

	math::AABB3 GltfMeshComponent::GetModelBox() const
	{
		C_P(const GltfMeshComponent);
		return d->Model.GetModelBox();
	}


	void GltfMeshComponent::Tick(float deltaTime)
	{
		C_P(GltfMeshComponent);
		d->TotalDeltaTime += deltaTime / 5.f;
		d->Model.Play(d->TotalDeltaTime, deltaTime);
	}

	void GltfMeshComponent::OnUpdateWorldTransform(float deltaTime)
	{
		C_P(GltfMeshComponent);
		auto& RootNodes = d->Model.GetSkeleton()->GetRootNode();
		if (!RootNodes.empty())
		{
			math::Matrix4x4 WorldTransform = GetOwner()->GetWorldTransform();
			RootNodes[0]->ParentMat = WorldTransform;
		}
	}

	bool GltfMeshComponent::GatherMesh(GltfSceneMeshInfo& SceneMeshInfo, std::shared_ptr<CameraComponent> Camera)
	{
		C_P(GltfMeshComponent);
		SceneMeshInfo.WorldTransform = GetOwner()->GetWorldTransform();
		SceneMeshInfo.PrevWorldTransform = GetOwner()->GetPrevWorldTransform();
		math::AABB3 Box = d->Model.GetModelBox().Transform(SceneMeshInfo.WorldTransform);
		bool Render = Camera->GetFrustum().Intersects(Box);
		if (Render)
		{
			auto& TmpMeshs = d->Model.GetModelMesh();
			std::for_each(TmpMeshs.begin(), TmpMeshs.end(), [&SceneMeshInfo](std::shared_ptr<GltfMesh> Item) {
				SceneMeshInfo.Meshes.push_back(Item);
				});
			
		}
		return Render;
	}

}
