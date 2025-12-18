#pragma once
#include "Scene/Component.h"
#include "math/vector3.h"
#include "Render/MaterialRender.h"

namespace Engine
{
	class MeshBase;
	class MaterialRender;
	class GltfModel;

	struct GltfMeshComponentPrivate;

	struct GltfSceneMeshInfo
	{
		std::vector<std::shared_ptr<MeshBase>> Meshes;
		math::Matrix4x4 WorldTransform;
		math::Matrix4x4 PrevWorldTransform;
	};

	class GltfMeshComponent : public Component
	{
	public:
		DECLARE_COMPONENT_CLASS_NAME(GltfMeshComponent)
		GltfMeshComponent(class std::weak_ptr<Actor> Owner);
		~GltfMeshComponent();

		bool Load(const std::wstring& FileName);
		bool Load(const nlohmann::json& GltfJson);
		GltfModel& GetModel() const;
		math::AABB3 GetModelBox() const;

		virtual void Tick(float deltaTime) override;
		virtual void OnUpdateWorldTransform(float deltaTime) override;

		bool GatherMesh(GltfSceneMeshInfo& SceneMeshInfo, std::shared_ptr<CameraComponent> Camera);
	private:
		GltfMeshComponentPrivate* d_ptr = nullptr;
	};

	DECLARE_COMPONENT_TRAITS_CLASS_NAME(GltfMeshComponent)
}

