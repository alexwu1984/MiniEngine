#pragma once
#include "RHI/RHICommandContext.h"
#include "Render/MaterialRender.h"
#include "GltfModel/DynamicBoneInfo.h"

namespace Engine
{
	struct BasePassRenderPrivate;
	class  GltfMesh;
	class  CameraComponent;
	class  SceneView;
	class  MaterialRender;
	struct GltfSceneMeshInfo;

	class BasePassRender
	{
	public:
		BasePassRender();
		~BasePassRender();

		void Render(const std::vector<GltfSceneMeshInfo> &MeshesPair,
			RenderCore::RHICommandContext& RHIContext,std::shared_ptr<SceneView> View);

	private:
		//对Mesh进行排序，按顺序渲染
		void SortMesh(const std::vector<GltfSceneMeshInfo>& MeshesPair,const math::Vector3& CameraPos);
		void ActualDraw(const std::vector<GltfSceneMeshInfo>& MeshesPair,
						RenderCore::RHICommandContext& RHIContext, 
						std::shared_ptr<CameraComponent> Camera, bool IsPreDraw);
		void DrawMesh(std::shared_ptr<GltfMesh> Mesh, const math::Matrix4x4& WorldTransform, 
						const math::Matrix4x4& PrevWorldTransform, 
						std::shared_ptr<MaterialRender> Render, RenderCore::RHICommandContext& RHIContext,
					 std::shared_ptr<CameraComponent> Camera, bool IsPreDraw);
		std::shared_ptr<MaterialRender> GetOrCreateRender(std::shared_ptr<GltfMesh> Mesh);
	private:
		BasePassRenderPrivate* d_ptr;
	};
}