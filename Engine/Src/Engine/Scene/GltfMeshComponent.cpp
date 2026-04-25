#include "Scene/GltfMeshComponent.h"
#include "GltfModel/GltfModel.h"
#include "GltfModel/GltfMesh.h"
#include "ObjModel/ObjMesh.h"
#include "ObjModel/ObjModel.h"
#include "Material/GltfMaterial.h"
#include "GltfModel/GltfSkeleton.h"
#include "GltfModel/GltfModelConfig.h"
#include "Render/PBRMaterialRender.h"
#include "Render/FurMaterialRender.h"
#include "RHI/RHICommandContext.h"
#include "RHI/DynamicRHI.h"
#include "Procedural/ProceduralFloor.h"
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
		GltfModel gltfModel;
		ObjModel objModel;
		bool isGltfModel = true;
		bool isProcedural = false;
		float TotalDeltaTime = 0.f;
		std::shared_ptr< GltfModelConfig>  ModelConfig;
		std::vector<std::shared_ptr<MeshBase>> ProceduralMeshes;
		math::AABB3 ProceduralBox;
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
				if (Path.find(L".glb") != std::wstring::npos)
				{
					if (!d->gltfModel.Load(Path, d->ModelConfig))
						return false;
				}
				else
				{
					d->isGltfModel = false;
					if (!d->objModel.Load(Path, d->ModelConfig))
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
			if (!d->gltfModel.Load(FileName,nullptr))
			{
				return false;
			}
		}

		return true;
	}

	bool GltfMeshComponent::Load(const nlohmann::json& GltfJson)
	{
		C_P(GltfMeshComponent);

		// Procedural floor path: replaces external floor.glb
		try
		{
			if (GltfJson.find("ProceduralFloor") != GltfJson.end())
			{
				ProceduralBuildResult BuildResult;
				if (BuildProceduralFloor(GltfJson["ProceduralFloor"], BuildResult))
				{
					d->ProceduralMeshes = std::move(BuildResult.Meshes);
					d->ProceduralBox = BuildResult.Box;
					d->isProcedural = true;
					d->isGltfModel = false;
					return true;
				}
			}
		}
		catch (...)
		{
		}

		d->ModelConfig = std::make_shared<GltfModelConfig>(std::static_pointer_cast<GltfMeshComponent>(this->shared_from_this()));
		if (d->ModelConfig->Load(GltfJson))
		{
			std::wstring Path = GEngine->GetModelPath();
			Path += L"/" + d->ModelConfig->GetModelName();
			if (Path.find(L".glb") != std::wstring::npos)
			{
				if (!d->gltfModel.Load(Path, d->ModelConfig))
					return false;
			}
			else
			{
				d->isGltfModel = false;
				if (!d->objModel.Load(Path, d->ModelConfig))
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
		return d->gltfModel;
	}

	math::AABB3 GltfMeshComponent::GetModelBox() const
	{
		C_P(const GltfMeshComponent);
		if (d->isProcedural)
		{
			return d->ProceduralBox;
		}
		return d->gltfModel.GetModelBox();
	}


	void GltfMeshComponent::Tick(float deltaTime)
	{
		C_P(GltfMeshComponent);
		d->TotalDeltaTime += deltaTime;
		if (d->isProcedural)
		{
			return;
		}
		if (d->isGltfModel)
		{
			d->gltfModel.Play(d->TotalDeltaTime, deltaTime);
		}
		
	}

	void GltfMeshComponent::OnUpdateWorldTransform(float deltaTime)
	{
		C_P(GltfMeshComponent);
		if (d->isProcedural)
		{
			return;
		}
		if (d->isGltfModel)
		{
			auto& RootNodes = d->gltfModel.GetSkeleton()->GetRootNode();
			if (!RootNodes.empty())
			{
				math::Matrix4x4 WorldTransform = GetOwner()->GetWorldTransform();
				RootNodes[0]->ParentMat = WorldTransform;
			}
		}

	}

	bool GltfMeshComponent::GatherMesh(GltfSceneMeshInfo& SceneMeshInfo, std::shared_ptr<CameraComponent> Camera)
	{
		C_P(GltfMeshComponent);
		SceneMeshInfo.WorldTransform = GetOwner()->GetWorldTransform();
		SceneMeshInfo.PrevWorldTransform = GetOwner()->GetPrevWorldTransform();
		if (d->isProcedural)
		{
			math::AABB3 Box = d->ProceduralBox.Transform(SceneMeshInfo.WorldTransform);
			bool Render = Camera->GetFrustum().Intersects(Box);
			if (Render)
			{
				for (auto& Mesh : d->ProceduralMeshes)
				{
					SceneMeshInfo.Meshes.push_back(Mesh);
				}
			}
			return Render;
		}
		if (d->isGltfModel)
		{
			math::AABB3 Box = d->gltfModel.GetModelBox().Transform(SceneMeshInfo.WorldTransform);
			bool Render = Camera->GetFrustum().Intersects(Box);
			if (Render)
			{
				auto& TmpMeshs = d->gltfModel.GetModelMesh();
				std::for_each(TmpMeshs.begin(), TmpMeshs.end(), [&SceneMeshInfo](std::shared_ptr<GltfMesh> Item) {
					SceneMeshInfo.Meshes.push_back(Item);
					});

			}
			return Render;
		}
		else
		{
			math::AABB3 Box = d->objModel.GetModelBox().Transform(SceneMeshInfo.WorldTransform);
			bool Render = Camera->GetFrustum().Intersects(Box);
			if (Render)
			{
				auto& TmpMeshs = d->objModel.GetModelMesh();
				std::for_each(TmpMeshs.begin(), TmpMeshs.end(), [&SceneMeshInfo](std::shared_ptr<ObjMesh> Item) {
					SceneMeshInfo.Meshes.push_back(Item);
					});

			}
			return Render;
		}

	}

}
