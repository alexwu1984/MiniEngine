#pragma once
#include "Scene/Component.h"
#include "math/frustum.h"
#include "math/vector3.h"
#include "Render/MaterialRender.h"

namespace Engine
{
	class MeshBase;
	class MaterialRender;
	class GltfModel;

	struct SceneMeshComponentPrivate;

	struct GltfSceneMeshInfo
	{
		std::vector<std::shared_ptr<MeshBase>> Meshes;
		/** Parallel to Meshes; stable logical slot for MaterialRender cache (Actor + Component + ordinal + Material id). */
		std::vector<uint64_t> MeshMaterialRenderCacheKeys;
		math::Matrix4x4 WorldTransform;
		math::Matrix4x4 PrevWorldTransform;
	};

	// Component that can load a scene model from JSON (glTF, Assimp-backed, procedural).
	class SceneMeshComponent : public Component
	{
	public:
		DECLARE_COMPONENT_CLASS_NAME(SceneMeshComponent)
		SceneMeshComponent(class std::weak_ptr<Actor> Owner);
		~SceneMeshComponent();

		bool Load(const std::wstring& FileName);
		bool Load(const nlohmann::json& ModelJson);
		GltfModel& GetModel() const;
		math::AABB3 GetModelBox() const;

		virtual void Tick(float deltaTime) override;
		virtual void OnUpdateWorldTransform(float deltaTime) override;

		/** @param ViewCullFrustum If null, fills SceneMeshInfo without view-frustum rejection (for shadow casters outside the camera frustum). */
		bool GatherMesh(GltfSceneMeshInfo& SceneMeshInfo, const math::Frustum* ViewCullFrustum);

		void SetProjectShadow(bool projShadow);
		bool IsProjectShadow() const;

	private:
		SceneMeshComponentPrivate* d_ptr = nullptr;
	};

	DECLARE_COMPONENT_TRAITS_CLASS_NAME(SceneMeshComponent)
}

