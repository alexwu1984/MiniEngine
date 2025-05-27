#pragma once
#include "RHI/RHICommandContext.h"
#include "Render/MaterialRender.h"
#include "GltfModel/DynamicBoneInfo.h"

namespace Engine
{
	struct BasePassRenderPrivate;
	class  MeshBase;
	class  CameraComponent;
	class  SceneView;
	class  MaterialRender;
	struct GltfSceneMeshInfo;
	struct MeshDistanceInfo;

	class BasePassRender
	{
	public:
		BasePassRender();
		~BasePassRender();

		void Render(RenderCore::DynamicRHI* RHI,const std::vector<GltfSceneMeshInfo> &MeshesPair,std::shared_ptr<SceneView> View);
		void SetIBLRotate(float x, float y);
	private:
		//对Mesh进行排序，按顺序渲染
		void SortMesh(const std::vector<GltfSceneMeshInfo>& MeshesPair, const math::Vector3& CameraPos);
		void SortMesh(const std::vector<GltfSceneMeshInfo>& MeshesPair, const math::Vector3& CameraPos, std::vector<MeshDistanceInfo>& Result);
		void GraphMeshByDistance(const GltfSceneMeshInfo& MeshInfo, const math::Vector3& CameraPos, std::vector<MeshDistanceInfo>& Result);
		void ActualDraw(RenderCore::DynamicRHI* RHI,const std::vector<GltfSceneMeshInfo>& MeshesPair,
						std::shared_ptr<CameraComponent> Camera, bool IsPreDraw);
		void DrawMesh(RenderCore::DynamicRHI* RHI,
					  std::shared_ptr<MeshBase> Mesh, const math::Matrix4x4& WorldTransform,
					  const math::Matrix4x4& PrevWorldTransform,
					  std::shared_ptr<MaterialRender> Render,
					  std::shared_ptr<CameraComponent> Camera, bool IsPreDraw);
		std::shared_ptr<MaterialRender> GetOrCreateRender(std::shared_ptr<MeshBase> Mesh);
	private:
		BasePassRenderPrivate* d_ptr = nullptr;
	};
}