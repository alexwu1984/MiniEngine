#include "Render/BasePassRender.h"
#include "GltfModel/GltfMesh.h"
#include "GltfModel/GltfMaterial.h"
#include "Scene/CameraComponent.h"
#include "Engine/Render/SceneRender.h"
#include "Engine/Engine.h"
#include "Render/PBRMaterialRender.h"
#include "Render/FurMaterialRender.h"
#include "Thread/RenderThread.h"

namespace Engine
{
	struct MeshDistanceInfo
	{
		float Distance;
		std::shared_ptr<GltfMesh> Mesh;
		math::Matrix4x4 WorldTransform;
		bool operator()(const MeshDistanceInfo& Near, const MeshDistanceInfo& Far)
		{
			return Near.Distance > Far.Distance;
		}
	};

	struct BasePassRenderPrivate
	{
		std::vector<MeshDistanceInfo> SortMesh;
		std::map<std::string, std::shared_ptr<MaterialRender>> Renders;
	};

	BasePassRender::BasePassRender()
		:d_ptr( new BasePassRenderPrivate())
	{

	}

	BasePassRender::~BasePassRender()
	{
		delete d_ptr;
	}

	void BasePassRender::Render(const std::vector<std::pair<std::vector<std::shared_ptr<GltfMesh>>, math::Matrix4x4>>& MeshesPair, RenderCore::RHICommandContext& RHIContext,
								std::shared_ptr<CameraComponent> Camera)
	{
		C_P(BasePassRender);
		d->SortMesh.clear();
		ActualDraw(MeshesPair,RHIContext, Camera, true);
		ActualDraw(MeshesPair,RHIContext, Camera, false);
	}

	void BasePassRender::SortMesh(const std::vector<std::pair<std::vector<std::shared_ptr<GltfMesh>>, math::Matrix4x4>>& MeshesPair,const math::Vector3& CameraPos)
	{
		C_P(BasePassRender);
		
		math::Matrix4x4 ModelMatrix;

		for (const auto& PairItem : MeshesPair)
		{
			size_t MeshSize = PairItem.first.size();
			for (int32_t MeshIndex = 0; MeshIndex < MeshSize; ++MeshIndex)
			{
				MeshDistanceInfo DisInfo;
				std::shared_ptr<GltfMesh> Mesh = PairItem.first[MeshIndex];

				math::Vector3 BoxPoint[8]{};
				Mesh->GetBoundingBox().GetPoint(BoxPoint);

				float distanceMin = 100000.f;
				float distanceMax = 0.f;
				DisInfo.WorldTransform = PairItem.second;
				DisInfo.Mesh = Mesh;

				for (int32_t PointIndex = 0; PointIndex < 8; PointIndex++)
				{
					math::Vector3 Point = BoxPoint[PointIndex];

					math::Vector4 Point4 = math::Vector4(Point.x, Point.y, Point.z, 1.0f);
					math::Vector4 TargetPoint = Point4 * Mesh->GetMeshMat() * ModelMatrix;
					TargetPoint = TargetPoint / TargetPoint.w;

					//计算两个向量Z的距离
					float Distance = math::Abs(TargetPoint.z - CameraPos.z);

					if (Distance < distanceMin)
					{
						distanceMin = Distance;
						DisInfo.Distance = distanceMin;
					}
					if (Distance > distanceMax)
					{
						distanceMax = Distance;
						DisInfo.Distance = distanceMax;
					}
				}
				d->SortMesh.push_back(DisInfo);
			}
		}


		std::sort(d->SortMesh.begin(), d->SortMesh.end(), MeshDistanceInfo());
	}

	void BasePassRender::ActualDraw(const std::vector<std::pair<std::vector<std::shared_ptr<GltfMesh>>, math::Matrix4x4>>& MeshesPair,
									RenderCore::RHICommandContext& RHIContext, 
									std::shared_ptr<CameraComponent> Camera, bool IsPreDraw)
	{
		C_P(BasePassRender);

		for (const auto& PairItem : MeshesPair)
		{
			size_t MeshSize = PairItem.first.size();
			for (int32_t MeshIndex = 0; MeshIndex < MeshSize; ++MeshIndex)
			{
				std::shared_ptr<GltfMesh> Mesh = PairItem.first[MeshIndex];

				auto Material = GetOrCreateRender(Mesh);

				if (!Mesh->GetMaterial()->IsTransparent())
				{
					DrawMesh(Mesh, PairItem.second, Material, RHIContext, Camera, IsPreDraw);
				}
			}
		}

		SortMesh(MeshesPair, Camera->GetCameraPos());

		for (const auto& SortItem : d->SortMesh)
		{
			std::shared_ptr<GltfMesh> Mesh = SortItem.Mesh;

			auto Material = GetOrCreateRender(Mesh);

			if (Mesh->GetMaterial()->IsTransparent())
			{
				DrawMesh(Mesh, SortItem.WorldTransform, Material, RHIContext, Camera, IsPreDraw);
			}
		}
	}

	void BasePassRender::DrawMesh(std::shared_ptr<GltfMesh> Mesh, const math::Matrix4x4& WorldTransform, 
		std::shared_ptr<MaterialRender> Render, RenderCore::RHICommandContext& RHIContext,
		std::shared_ptr<CameraComponent> Camera, bool IsPreDraw)
	{
		C_P(BasePassRender);
		MaterialRenderParam RenderParam;
		RenderParam.CameraPos = Camera->GetCameraPos();
		RenderParam.CurrModelMatrix = Mesh->GetMeshMat() * WorldTransform;
		RenderParam.CurrViewProjMatrix = Camera->GetViewMatrix() * Camera->GetProjMatrix();
		RenderParam.CurrViewProjInverseMatrix = RenderParam.CurrViewProjMatrix.Inverse();
		RenderParam.HasSkin = Mesh->HasSkin();
		RenderParam._PreProcessor = GEngine->GetSceneRender()->GetPreProcessor();

		auto RenderMesh = [Render, RenderParam, Mesh, IsPreDraw](RenderCore::DynamicRHI* DyRHI)
			{
				if (Mesh->GetSkinId() > -1 && Mesh->GetBoneNodeArray().size() > 0)
				{
					auto& Bone = Mesh->GetBoneNodeArray()[Mesh->GetSkinId()];
					for (uint32_t BoneIndex = 0; BoneIndex < Bone.size(); BoneIndex++)
					{
						Render->SetBoneMatrix(Bone[BoneIndex].FinalMat, BoneIndex);
					}
				}
				if (IsPreDraw)
				{
					Render->PreDraw(*DyRHI->GetDefaultCommandContext(), RenderParam);
				}
				else
				{
					Render->Draw(*DyRHI->GetDefaultCommandContext(), RenderParam);
				}

			};

		ENQUEUE_UNIQUE_RENDER_COMMAND(RenderMesh);

	}

	std::shared_ptr<Engine::MaterialRender> BasePassRender::GetOrCreateRender(std::shared_ptr<GltfMesh> Mesh)
	{
		C_P(BasePassRender);
		
		auto ItFind = d->Renders.find(Mesh->GetMeshName());
		if (ItFind != d->Renders.end())
		{
			return ItFind->second;
		}
		std::shared_ptr<PBRMaterialRender> PBRMaterial;
		switch (Mesh->GetMaterial()->GetMaterialType())
		{
		case GltfMaterial::MaterialType::PBR:
			PBRMaterial = std::make_shared<PBRMaterialRender>(Mesh->GetMeshBuffer(), Mesh->GetMaterial());
			break;
		case GltfMaterial::MaterialType::FUR:
			PBRMaterial = std::make_shared<FurMaterialRender>(Mesh->GetMeshBuffer(), Mesh->GetMaterial());
			break;
		default:
			PBRMaterial = std::make_shared<PBRMaterialRender>(Mesh->GetMeshBuffer(), Mesh->GetMaterial());
		}

		PBRMaterial->InitRenderResource();
		d->Renders.insert({ Mesh->GetMeshName(),PBRMaterial });
		return PBRMaterial;
	}

}