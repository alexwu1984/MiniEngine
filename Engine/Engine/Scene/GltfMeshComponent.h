#pragma once
#include "Scene/Component.h"
#include "math/vector3.h"

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

		virtual void Draw(RenderCore::RHICommandContext& RHIContext, std::shared_ptr<CameraComponent> Camera);

		virtual void Tick(float deltaTime) override;
		virtual void OnUpdateWorldTransform(float deltaTime) override;
	private:
		void ActualDraw(RenderCore::RHICommandContext& RHIContext, std::shared_ptr<CameraComponent> Camera,bool IsPreDraw);
	
		//对Mesh进行排序，按顺序渲染
		void SortMesh(const math::Vector3& CameraPos);
		void DrawMesh(std::shared_ptr<GltfMesh> Mesh, const math::Matrix4x4& WorldTransform, std::shared_ptr<MaterialRender> Render, 
			std::shared_ptr<CameraComponent> Camera,int32_t PosType,bool IsPreDraw);
	private:
		std::shared_ptr< GltfMeshComponentP> Impl;
	};

	DECLARE_COMPONENT_TRAITS_CLASS_NAME(GltfMeshComponent)
}

