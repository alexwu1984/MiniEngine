#include "Scene/GltfMeshComponent.h"
#include "GltfModel/GltfModel.h"
#include "GltfModel/GltfMesh.h"
#include "GltfModel/GltfMaterial.h"
#include "Render/PBRMaterialRender.h"
#include "RHI/RHICommandContext.h"
#include "RHI/DynamicRHI.h"
#include "Scene/Actor.h"
#include "Scene/CameraComponent.h"
#include "Thread/RenderThread.h"

namespace Engine
{
	struct MeshDistanceInfo
	{
		float Distance;
		int MeshID;

		bool operator()(const MeshDistanceInfo& Near, const MeshDistanceInfo& Far)
		{
			return Near.Distance > Far.Distance;
		}
	};

	struct GltfMeshComponentP
	{
		GltfModel Model;
		std::map<int32_t, std::shared_ptr<MaterialRender>> Renders;
		std::vector<MeshDistanceInfo> SortMesh;
	};

	GltfMeshComponent::GltfMeshComponent(std::weak_ptr<Actor> Owner)
		:Component(Owner)
		, Impl(std::make_shared<GltfMeshComponentP>())
	{
		
	}

	GltfMeshComponent::~GltfMeshComponent()
	{
		if (GRenderThread)
		{
			GRenderThread->WaitForFinish();
		}
	}

	//Todo: load json config
	bool GltfMeshComponent::Load(const std::wstring& FileName)
	{
		if (!Impl->Model.Load(FileName))
		{
			return false;
		}

		size_t MeshSize = Impl->Model.GetModelMesh().size();

		for (size_t MeshIndex = 0; MeshIndex < MeshSize; ++MeshIndex)
		{
			std::shared_ptr<GltfMesh> Mesh = Impl->Model.GetModelMesh()[MeshIndex];
			//default Material
			std::shared_ptr<PBRMaterialRender> PBRMaterial = std::make_shared<PBRMaterialRender>(Mesh->GetMeshBuffer(),Mesh->GetMaterial());
			nlohmann::json jsonObj;
			PBRMaterial->InitRenderResource(jsonObj);
			Impl->Renders.insert({MeshIndex,PBRMaterial});
		}

		return true;
	}

	void GltfMeshComponent::Draw(RenderCore::RHICommandContext& RHIContext, std::shared_ptr<CameraComponent> Camera)
	{
		math::AABB3 Box = Impl->Model.GetModelBox().Transform(GetOwner()->GetWorldTransform());
		bool Render = Camera->GetFrustum().Intersects(Box);
		if (!Render)
		{
			return;
		}

		//Draw opacity mesh at first 
		size_t MeshSize = Impl->Model.GetModelMesh().size();
		for (size_t MeshIndex = 0; MeshIndex < MeshSize; ++MeshIndex)
		{
			std::shared_ptr<GltfMesh> Mesh = Impl->Model.GetModelMesh()[MeshIndex];
			if (!Mesh->GetMaterial()->IsTransparent())
			{
				DrawMesh(Mesh, Impl->Renders[MeshIndex], Camera);
			}
		}

		SortMesh(Camera->GetCameraPos());

		MeshSize = Impl->SortMesh.size();

		for (size_t MeshIndex = 0; MeshIndex < MeshSize; ++MeshIndex)
		{
			int MeshID = Impl->SortMesh[MeshIndex].MeshID;
			std::shared_ptr<GltfMesh> Mesh = Impl->Model.GetModelMesh()[MeshID];
			if (Mesh->GetMaterial()->IsTransparent())
			{
				DrawMesh(Mesh, Impl->Renders[MeshID], Camera);
			}
		}

	}

	void GltfMeshComponent::SortMesh(const math::Vector3& CameraPos)
	{
		int MeshSize = Impl->Model.GetModelMesh().size();
		if (MeshSize == 0)
		{
			return;
		}
		if (Impl->SortMesh.size() != MeshSize)
		{
			Impl->SortMesh.resize(MeshSize);
		}
		MeshDistanceInfo* SortMesh = &Impl->SortMesh[0];

		MeshDistanceInfo DisInfo;

		math::Matrix4x4 ModelMatrix;

		for (int32_t MeshIndex = 0; MeshIndex < MeshSize; MeshIndex++)
		{
			DisInfo.MeshID = MeshIndex;
			std::shared_ptr<GltfMesh> Mesh = Impl->Model.GetModelMesh()[MeshIndex];

			math::Vector3 BoxPoint[8]{};
			Mesh->GetBoundingBox().GetPoint(BoxPoint);

			float distanceMax = 0.f;

			for (int32_t PointIndex = 0; PointIndex < 8; PointIndex++)
			{
				math::Vector3 Point = BoxPoint[PointIndex];

				math::Vector4 Point4 = math::Vector4(Point.x, Point.y, Point.z, 1.0f);
				math::Vector4 TargetPoint = Point4 * Mesh->GetMeshMat() * ModelMatrix;
				TargetPoint = TargetPoint / TargetPoint.w;

				//计算两个向量Z的距离
				float Distance = math::Abs(TargetPoint.z - CameraPos.z);
				DisInfo.Distance = Distance;
				if (distanceMax < Distance)
				{
					distanceMax = Distance;
					SortMesh[MeshIndex] = DisInfo;
				}

			}
		}
		std::sort(Impl->SortMesh.begin(), Impl->SortMesh.end(), MeshDistanceInfo());
	}

	void GltfMeshComponent::DrawMesh(std::shared_ptr<GltfMesh> Mesh, std::shared_ptr<MaterialRender> Render, std::shared_ptr<CameraComponent> Camera)
	{
		if (!Render)
		{
			return;
		}
		MaterialRenderParam RenderParam;
		RenderParam.CameraPos = Camera->GetCameraPos();
		RenderParam.CurrModelMatrix = Mesh->GetMeshMat();
		RenderParam.CurrViewProjMatrix = Camera->GetViewMatrix() * Camera->GetProjMatrix();
		RenderParam.CurrViewProjInverseMatrix = RenderParam.CurrViewProjMatrix.Inverse();

		auto RenderMesh = [Render = Render, RenderParam, Mesh = Mesh](RenderCore::DynamicRHI* DyRHI)
		{
			Render->Draw(*DyRHI->GetDefaultCommandContext(), RenderParam);
		};

		ENQUEUE_UNIQUE_RENDER_COMMAND(RenderMesh);

	}

}
