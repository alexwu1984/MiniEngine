#include "Scene/SceneMeshComponent.h"
#include "GltfModel/GltfModel.h"
#include "GltfModel/GltfMesh.h"
#include "AssimpModel/AssimpMesh.h"
#include "AssimpModel/AssimpModel.h"
#include "Material/GltfMaterial.h"
#include "GltfModel/GltfSkeleton.h"
#include "Scene/SceneModelAsset.h"
#include "Render/PBRMaterialRender.h"
#include "Render/FurMaterialRender.h"
#include "RHI/RHICommandContext.h"
#include "RHI/DynamicRHI.h"
#include "Procedural/ProceduralFloor.h"
#include "Procedural/ProceduralModel.h"
#include "Scene/Actor.h"
#include "Scene/CameraComponent.h"
#include "Scene/World.h"
#include "Scene/FScene.h"
#include "Render/SceneRendering/MeshMaterialRenderCache.h"
#include "Thread/RenderThread.h"
#include "core/logger.h"
#include "Engine/Engine.h"
#include "Engine/JsonConfig.h"
#include "Render/WorldSceneRender.h"
#include "Render/RenderStableIds.h"
#include <variant>

namespace Engine
{
	IMP_COMPONENT_CLASS_NAME(SceneMeshComponent)
	IMP_COMPONENT_TRAITS_CLASS_NAME(SceneMeshComponent)

	struct SceneMeshComponentPrivate
	{
		using ModelVariant = std::variant<GltfModel, AssimpModel, ProceduralModel>;
		ModelVariant Model;
		float TotalDeltaTime = 0.f;
		std::shared_ptr<SceneModelAsset> Asset;
		bool bProjectShadow = false;
	};

	namespace
	{
		bool TryGetDrawableMeshAtOrdinal(SceneMeshComponentPrivate* D, uint32_t Ordinal, std::shared_ptr<MeshBase>& OutMesh)
		{
			if (!D)
				return false;
			if (const auto PM = std::get_if<ProceduralModel>(&D->Model))
			{
				if (Ordinal >= PM->Meshes.size())
					return false;
				OutMesh = PM->Meshes[static_cast<size_t>(Ordinal)];
				return static_cast<bool>(OutMesh);
			}
			if (const auto GM = std::get_if<GltfModel>(&D->Model))
			{
				const auto& List = GM->GetModelMesh();
				if (Ordinal >= List.size())
					return false;
				OutMesh = List[static_cast<size_t>(Ordinal)];
				return static_cast<bool>(OutMesh);
			}
			if (const auto OM = std::get_if<AssimpModel>(&D->Model))
			{
				const auto& List = OM->GetModelMesh();
				if (Ordinal >= List.size())
					return false;
				OutMesh = List[static_cast<size_t>(Ordinal)];
				return static_cast<bool>(OutMesh);
			}
			return false;
		}
	} // namespace

	SceneMeshComponent::SceneMeshComponent(std::weak_ptr<Actor> Owner)
		:Component(Owner)
		, d_ptr(new SceneMeshComponentPrivate())
	{
		
	}

	SceneMeshComponent::~SceneMeshComponent()
	{
		if (GRenderThread && std::this_thread::get_id() != GRenderThread->GetWorkerThreadId())
			GRenderThread->WaitForFinish();
		delete d_ptr;
	}

	//Todo: load json config
	bool SceneMeshComponent::Load(const std::wstring& FileName)
	{
		C_P(SceneMeshComponent);
		std::filesystem::path Path = FileName;
		if (!Path.has_extension())
		{
			core::err() << __FUNCTION__ << " Load File failed:" << FileName;
			return false;
		}
		std::wstring Extension =  Path.extension().wstring();

		if (Extension == L".json")
		{
			d->Asset = std::make_shared<SceneModelAsset>();
			// Legacy entry-point: this path expects the JSON to be the per-model config.
			// Prefer World::LoadScene model entries going forward.
			nlohmann::json Root;
			if (!LoadJsonFile(FileName, Root))
				return false;
			if (!d->Asset->Load(Root))
			{
				return false;
			}
			{
				std::wstring Path = std::filesystem::path(FileName).parent_path().wstring();
				Path += L"/" + d->Asset->GetModelRelativePath();
				if (Path.find(L".glb") != std::wstring::npos)
				{
					d->Model.emplace<GltfModel>();
					if (!std::get<GltfModel>(d->Model).Load(Path, d->Asset))
						return false;
				}
				else
				{
					d->Model.emplace<AssimpModel>();
					if (!std::get<AssimpModel>(d->Model).Load(Path, d->Asset))
						return false;
				}
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

	bool SceneMeshComponent::Load(const nlohmann::json& ModelJson)
	{
		C_P(SceneMeshComponent);

		// Procedural floor path: replaces external floor.glb
		try
		{
			if (ModelJson.find("ProceduralFloor") != ModelJson.end())
			{
				ProceduralBuildResult BuildResult;
				if (BuildProceduralFloor(ModelJson["ProceduralFloor"], BuildResult))
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

		d->Asset = std::make_shared<SceneModelAsset>();
		if (d->Asset->Load(ModelJson))
		{
			std::wstring Path = GEngine->GetModelPath();
			Path += L"/" + d->Asset->GetModelRelativePath();
			if (Path.find(L".glb") != std::wstring::npos)
			{
				d->Model.emplace<GltfModel>();
				if (!std::get<GltfModel>(d->Model).Load(Path, d->Asset))
					return false;
			}
			else
			{
				d->Model.emplace<AssimpModel>();
				if (!std::get<AssimpModel>(d->Model).Load(Path, d->Asset))
					return false;
			}
			return true;
		}
		else
		{
			return false;
		}
	}

	GltfModel& SceneMeshComponent::GetModel() const
	{
		C_P(SceneMeshComponent);
		// Backward compatibility: this component historically exposed gltfModel directly.
		// If current model isn't GLTF, return a default-constructed temporary stored in variant.
		if (!std::holds_alternative<GltfModel>(d->Model))
		{
			const_cast<SceneMeshComponentPrivate*>(d)->Model.emplace<GltfModel>();
		}
		return std::get<GltfModel>(d->Model);
	}

	math::AABB3 SceneMeshComponent::GetModelBox() const
	{
		C_P(const SceneMeshComponent);
		if (auto PM = std::get_if<ProceduralModel>(&d->Model))
			return PM->Box;
		if (auto GM = std::get_if<GltfModel>(&d->Model))
			return GM->GetModelBox();
		if (auto OM = std::get_if<AssimpModel>(&d->Model))
			return OM->GetModelBox();
		return {};
	}


	void SceneMeshComponent::Tick(float deltaTime)
	{
		C_P(SceneMeshComponent);
		d->TotalDeltaTime += deltaTime;
		if (auto GM = std::get_if<GltfModel>(&d->Model))
		{
			GM->Play(d->TotalDeltaTime, deltaTime);
		}
		
	}

	void SceneMeshComponent::OnUpdateWorldTransform(float deltaTime)
	{
		C_P(SceneMeshComponent);
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

	std::vector<uint64_t> SceneMeshComponent::BuildMeshMaterialRenderCacheStableSlotKeys()
	{
		C_P(SceneMeshComponent);
		std::vector<uint64_t> Out;
		const std::shared_ptr<Actor> OwnerActor = GetOwner();
		if (!OwnerActor)
			return Out;
		const uint64_t ActorStable = OwnerActor->GetStableInstanceId();
		const uint64_t CompStable = GetStableComponentInstanceId();
		for (uint32_t Ordinal = 0;; ++Ordinal)
		{
			std::shared_ptr<MeshBase> M;
			if (!TryGetDrawableMeshAtOrdinal(d, Ordinal, M) || !M || !M->GetMaterial())
				break;
			Out.push_back(BuildMeshMaterialRenderCacheKey(ActorStable, CompStable, Ordinal, M->GetMaterial()->GetStableMaterialInstanceId()));
		}
		return Out;
	}

	void SceneMeshComponent::MarkMeshMaterialRenderResourcesDirty()
	{
		const std::shared_ptr<Actor> OwnerActor = GetOwner();
		if (!OwnerActor)
			return;
		const std::shared_ptr<World> W = OwnerActor->GetWorld();
		if (!W)
			return;
		const std::shared_ptr<FScene> ScenePtr = W->GetScene();
		if (!ScenePtr)
			return;
		const std::vector<uint64_t> SlotKeys = BuildMeshMaterialRenderCacheStableSlotKeys();
		if (SlotKeys.empty())
			return;
		std::weak_ptr<FScene> WeakScene = ScenePtr;
		ENQUEUE_UNIQUE_RENDER_COMMAND(
			[WeakScene, SlotKeys](RenderCore::DynamicRHI* RHI)
			{
				(void)RHI;
				std::shared_ptr<FScene> S = WeakScene.lock();
				if (!S)
					return;
				FMeshMaterialRenderCache* const Cache = S->GetMeshMaterialRenderCache();
				if (!Cache)
					return;
				for (const uint64_t K : SlotKeys)
				{
					if (K != 0)
						Cache->InvalidateByStableSlotKey(K);
				}
			},
			false);
		FlushRenderingCommands();
	}

	void SceneMeshComponent::MarkMeshMaterialRenderSlotDirty(uint32_t MeshOrdinalWithinComponent)
	{
		C_P(SceneMeshComponent);
		std::shared_ptr<MeshBase> M;
		if (!TryGetDrawableMeshAtOrdinal(d, MeshOrdinalWithinComponent, M) || !M || !M->GetMaterial())
			return;
		const std::shared_ptr<Actor> OwnerActor = GetOwner();
		if (!OwnerActor)
			return;
		const uint64_t SlotKey = BuildMeshMaterialRenderCacheKey(
			OwnerActor->GetStableInstanceId(),
			GetStableComponentInstanceId(),
			MeshOrdinalWithinComponent,
			M->GetMaterial()->GetStableMaterialInstanceId());
		const std::shared_ptr<World> W = OwnerActor->GetWorld();
		if (!W)
			return;
		const std::shared_ptr<FScene> ScenePtr = W->GetScene();
		if (!ScenePtr)
			return;
		std::weak_ptr<FScene> WeakScene = ScenePtr;
		ENQUEUE_UNIQUE_RENDER_COMMAND(
			[WeakScene, SlotKey](RenderCore::DynamicRHI* RHI)
			{
				(void)RHI;
				std::shared_ptr<FScene> S = WeakScene.lock();
				if (!S)
					return;
				if (FMeshMaterialRenderCache* Cache = S->GetMeshMaterialRenderCache())
					Cache->InvalidateByStableSlotKey(SlotKey);
			},
			false);
		FlushRenderingCommands();
	}

	bool SceneMeshComponent::GatherMesh(GltfSceneMeshInfo& SceneMeshInfo, const math::Frustum* ViewCullFrustum)
	{
		C_P(SceneMeshComponent);
		SceneMeshInfo.WorldTransform = GetOwner()->GetWorldTransform();
		SceneMeshInfo.PrevWorldTransform = GetOwner()->GetPrevWorldTransform();
		const std::shared_ptr<Actor> OwnerActor = GetOwner();
		const uint64_t ActorStable = OwnerActor ? OwnerActor->GetStableInstanceId() : 0u;
		const uint64_t CompStable = GetStableComponentInstanceId();

		auto PushDrawableMesh = [&](const std::shared_ptr<MeshBase>& M)
		{
			if (!M)
				return;
			const uint32_t Ordinal = static_cast<uint32_t>(SceneMeshInfo.Meshes.size());
			SceneMeshInfo.Meshes.push_back(M);
			SceneMeshInfo.MeshMaterialRenderCacheKeys.push_back(
				BuildMeshMaterialRenderCacheKey(ActorStable, CompStable, Ordinal, M->GetMaterial()->GetStableMaterialInstanceId()));
		};

		if (auto PM = std::get_if<ProceduralModel>(&d->Model))
		{
			math::AABB3 Box = PM->Box.Transform(SceneMeshInfo.WorldTransform);
			const bool Render = (ViewCullFrustum == nullptr) || ViewCullFrustum->Intersects(Box);
			if (Render)
			{
				for (auto& Mesh : PM->Meshes)
					PushDrawableMesh(Mesh);
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
				for (const auto& Item : TmpMeshs)
					PushDrawableMesh(Item);
			}
			return Render;
		}
		if (auto OM = std::get_if<AssimpModel>(&d->Model))
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
				for (const auto& Item : TmpMeshs)
					PushDrawableMesh(Item);
			}
			return Render;
		}
		return false;

	}

	void SceneMeshComponent::SetProjectShadow(bool projShadow)
	{
		C_P(SceneMeshComponent);
		if (d->bProjectShadow == projShadow)
			return;
		d->bProjectShadow = projShadow;
		if (auto Owner = GetOwner())
			if (auto W = Owner->GetWorld())
				W->RefreshShadowProjectorForActor(Owner);
	}

	bool SceneMeshComponent::IsProjectShadow() const
	{
		C_P(const SceneMeshComponent);
		return d->bProjectShadow;
	}

}
