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
#include "Procedural/ProceduralModel.h"
#include "Scene/Actor.h"
#include "Scene/CameraComponent.h"
#include "Scene/World.h"
#include "Thread/RenderThread.h"
#include "core/logger.h"
#include "Engine/Engine.h"
#include "Render/SceneRender.h"
#include <variant>

namespace Engine
{
	IMP_COMPONENT_CLASS_NAME(GltfMeshComponent)
	IMP_COMPONENT_TRAITS_CLASS_NAME(GltfMeshComponent)

	struct GltfMeshComponentPrivate
	{
		using ModelVariant = std::variant<GltfModel, ObjModel, ProceduralModel>;
		ModelVariant Model;
		float TotalDeltaTime = 0.f;
		std::shared_ptr< GltfModelConfig>  ModelConfig;
		bool bProjectShadow = false;
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
					d->Model.emplace<GltfModel>();
					if (!std::get<GltfModel>(d->Model).Load(Path, d->ModelConfig))
						return false;
				}
				else
				{
					d->Model.emplace<ObjModel>();
					if (!std::get<ObjModel>(d->Model).Load(Path, d->ModelConfig))
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
			d->Model.emplace<GltfModel>();
			if (!std::get<GltfModel>(d->Model).Load(FileName, nullptr))
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
					ProceduralModel PM;
					PM.Meshes = std::move(BuildResult.Meshes);
					PM.Box = BuildResult.Box;
					d->Model = std::move(PM);
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
				d->Model.emplace<GltfModel>();
				if (!std::get<GltfModel>(d->Model).Load(Path, d->ModelConfig))
					return false;
			}
			else
			{
				d->Model.emplace<ObjModel>();
				if (!std::get<ObjModel>(d->Model).Load(Path, d->ModelConfig))
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
		// Backward compatibility: this component historically exposed gltfModel directly.
		// If current model isn't GLTF, return a default-constructed temporary stored in variant.
		if (!std::holds_alternative<GltfModel>(d->Model))
		{
			const_cast<GltfMeshComponentPrivate*>(d)->Model.emplace<GltfModel>();
		}
		return std::get<GltfModel>(d->Model);
	}

	math::AABB3 GltfMeshComponent::GetModelBox() const
	{
		C_P(const GltfMeshComponent);
		if (auto PM = std::get_if<ProceduralModel>(&d->Model))
			return PM->Box;
		if (auto GM = std::get_if<GltfModel>(&d->Model))
			return GM->GetModelBox();
		if (auto OM = std::get_if<ObjModel>(&d->Model))
			return OM->GetModelBox();
		return {};
	}


	void GltfMeshComponent::Tick(float deltaTime)
	{
		C_P(GltfMeshComponent);
		d->TotalDeltaTime += deltaTime;
		if (auto GM = std::get_if<GltfModel>(&d->Model))
		{
			GM->Play(d->TotalDeltaTime, deltaTime);
		}
		
	}

	void GltfMeshComponent::OnUpdateWorldTransform(float deltaTime)
	{
		C_P(GltfMeshComponent);
		if (auto GM = std::get_if<GltfModel>(&d->Model))
		{
			auto Skel = GM->GetSkeleton();
			if (!Skel)
				return;
			// Every skin joint tree root must receive the actor world matrix.
			// Models with multiple roots (e.g. body + fur/secondary skin) previously only updated [0],
			// leaving other roots at identity → meshes skinned to those bones appear detached or culled.
			auto& RootNodes = Skel->GetRootNode();
			if (!RootNodes.empty())
			{
				const math::Matrix4x4 WorldTransform = GetOwner()->GetWorldTransform();
				for (const auto& Root : RootNodes)
				{
					if (Root)
						Root->ParentMat = WorldTransform;
				}
			}
		}

	}

	bool GltfMeshComponent::GatherMesh(GltfSceneMeshInfo& SceneMeshInfo, const math::Frustum* ViewCullFrustum)
	{
		C_P(GltfMeshComponent);
		SceneMeshInfo.WorldTransform = GetOwner()->GetWorldTransform();
		SceneMeshInfo.PrevWorldTransform = GetOwner()->GetPrevWorldTransform();
		if (auto PM = std::get_if<ProceduralModel>(&d->Model))
		{
			math::AABB3 Box = PM->Box.Transform(SceneMeshInfo.WorldTransform);
			const bool Render = (ViewCullFrustum == nullptr) || ViewCullFrustum->Intersects(Box);
			if (Render)
			{
				for (auto& Mesh : PM->Meshes)
				{
					if (Mesh)
						SceneMeshInfo.Meshes.push_back(Mesh);
				}
			}
			return Render;
		}
		if (auto GM = std::get_if<GltfModel>(&d->Model))
		{
			// Use merged per-mesh WORLD bounds for culling.
			// Reason: the final draw matrix is MeshMat * WorldTransform, and MeshMat may change at runtime
			// (e.g. node animation). Using only the precomputed model AABB can incorrectly cull animated meshes.
			math::AABB3 mergedWorldAabb;
			bool mergedValid = false;
			auto& TmpMeshs = GM->GetModelMesh();
			for (const auto& Mesh : TmpMeshs)
			{
				if (!Mesh) continue;
				math::AABB3 wbox = Mesh->GetBoundingBox().Transform(Mesh->GetMeshMat() * SceneMeshInfo.WorldTransform);
				mergedWorldAabb = mergedValid ? mergedWorldAabb.MergeAABB(wbox) : wbox;
				mergedValid = true;
			}
			// Fallback to model box if mesh list is empty.
			if (!mergedValid)
				mergedWorldAabb = GM->GetModelBox().Transform(SceneMeshInfo.WorldTransform);

			const bool Render = (ViewCullFrustum == nullptr) || ViewCullFrustum->Intersects(mergedWorldAabb);
			if (Render)
			{
				std::for_each(TmpMeshs.begin(), TmpMeshs.end(), [&SceneMeshInfo](std::shared_ptr<GltfMesh> Item) {
					if (Item)
						SceneMeshInfo.Meshes.push_back(Item);
					});

			}
			return Render;
		}
		if (auto OM = std::get_if<ObjModel>(&d->Model))
		{
			math::AABB3 mergedWorldAabb;
			bool mergedValid = false;
			auto& TmpMeshs = OM->GetModelMesh();
			for (const auto& Mesh : TmpMeshs)
			{
				if (!Mesh) continue;
				math::AABB3 wbox = Mesh->GetBoundingBox().Transform(Mesh->GetMeshMat() * SceneMeshInfo.WorldTransform);
				mergedWorldAabb = mergedValid ? mergedWorldAabb.MergeAABB(wbox) : wbox;
				mergedValid = true;
			}
			if (!mergedValid)
				mergedWorldAabb = OM->GetModelBox().Transform(SceneMeshInfo.WorldTransform);

			const bool Render = (ViewCullFrustum == nullptr) || ViewCullFrustum->Intersects(mergedWorldAabb);
			if (Render)
			{
				std::for_each(TmpMeshs.begin(), TmpMeshs.end(), [&SceneMeshInfo](std::shared_ptr<ObjMesh> Item) {
					if (Item)
						SceneMeshInfo.Meshes.push_back(Item);
					});

			}
			return Render;
		}
		return false;

	}

	void GltfMeshComponent::SetProjectShadow(bool projShadow)
	{
		C_P(GltfMeshComponent);
		if (d->bProjectShadow == projShadow)
			return;
		d->bProjectShadow = projShadow;
		if (auto Owner = GetOwner())
			if (auto W = Owner->GetWorld())
				W->RefreshShadowProjectorForActor(Owner);
	}

	bool GltfMeshComponent::IsProjectShadow() const
	{
		C_P(const GltfMeshComponent);
		return d->bProjectShadow;
	}

}
