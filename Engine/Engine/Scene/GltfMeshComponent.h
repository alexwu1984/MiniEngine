#pragma once
#include "Scene/Component.h"
#include "math/vector3.h"
#include "Render/MaterialRender.h"

namespace Engine
{
	class GltfMesh;
	class MaterialRender;
	class GltfModel;

	struct GltfMeshComponentP;

	class GltfMeshComponent : public Component
	{
	public:
		DECLARE_COMPONENT_CLASS_NAME(GltfMeshComponent)
		GltfMeshComponent(class std::weak_ptr<Actor> Owner);
		~GltfMeshComponent();

		bool Load(const std::wstring& FileName);
		GltfModel& GetModel() const;
		math::AABB3 GetModelBox() const;


		virtual void Tick(float deltaTime) override;
		virtual void OnUpdateWorldTransform(float deltaTime) override;

		bool GatherMesh(std::vector<std::shared_ptr<GltfMesh>>& Meshes, math::Matrix4x4& WorldTransform, std::shared_ptr<CameraComponent> Camera);
	private:
		std::shared_ptr< GltfMeshComponentP> Impl;
	};

	DECLARE_COMPONENT_TRAITS_CLASS_NAME(GltfMeshComponent)
}

