#include "Render/BasePassRender.h"
#include "GltfModel/GltfMesh.h"
#include "Material/GltfMaterial.h"
#include "Scene/GltfMeshComponent.h"
#include "Scene/CameraComponent.h"
#include "Engine/Render/SceneRender.h"
#include "Engine/Thread/RenderThread.h"
#include "Engine/Engine.h"
#include "Render/PBRMaterialRender.h"
#include "Render/FurMaterialRender.h"
#include "Material/GltfFurMaterial.h"
#include "Engine/Scene/SceneView.h"
#include "Engine/Render/PostProcessor.h"

namespace Engine
{
	struct MeshDistanceInfo
	{
		float Distance;
		std::shared_ptr<MeshBase> Mesh;
		math::Matrix4x4 WorldTransform;
		math::Matrix4x4 PrevWorldTransform;
		bool operator()(const MeshDistanceInfo& Near, const MeshDistanceInfo& Far)
		{
			return Near.Distance < Far.Distance;
		}
	};

	struct BasePassRenderPrivate
	{
		std::vector<MeshDistanceInfo> SortMesh;
		std::map<std::shared_ptr<MeshBase>, std::shared_ptr<MaterialRender>> Renders;
		std::shared_ptr<SceneView> sceneView;
		std::shared_ptr<GBuffer> TargetBuffer;
		float xHDRRotate{ 0.f };
		float yHDRRotate{ 1.f };
	};

	BasePassRender::BasePassRender()
		:d_ptr( new BasePassRenderPrivate())
	{
	}

	BasePassRender::~BasePassRender()
	{
		delete d_ptr;
	}

	void BasePassRender::Render(RenderCore::DynamicRHI* RHI,const std::vector<GltfSceneMeshInfo>& MeshInfos,std::shared_ptr<SceneView> View, std::shared_ptr<GBuffer> TargetBuffer)
	{
		C_P(BasePassRender);
		d->SortMesh.clear();
		d->sceneView = View;
		d->TargetBuffer = TargetBuffer;
		ActualDraw(RHI, MeshInfos, View->GetMainCamera(), true);
		ActualDraw(RHI, MeshInfos, View->GetMainCamera(), false);
	}

	void BasePassRender::SetIBLRotate(float x, float y)
	{
		C_P(BasePassRender);
		d->xHDRRotate = x;
		d->yHDRRotate = y;
	}

	void BasePassRender::SortMesh(const std::vector<GltfSceneMeshInfo>& MeshInfos, const math::Vector3& CameraPos)
	{
		C_P(BasePassRender);
		SortMesh(MeshInfos, CameraPos, d->SortMesh);
	}

	void BasePassRender::SortMesh(const std::vector<GltfSceneMeshInfo>& MeshInfos, const math::Vector3& CameraPos, std::vector<MeshDistanceInfo>& Result)
	{
		for (const auto& MeshInfo : MeshInfos)
		{
			GraphMeshByDistance(MeshInfo, CameraPos, Result);
		}
		std::sort(Result.begin(), Result.end(), MeshDistanceInfo());
	}

	void BasePassRender::GraphMeshByDistance(const GltfSceneMeshInfo& MeshInfo, const math::Vector3& CameraPos, std::vector<MeshDistanceInfo>& Result)
	{
		size_t MeshSize = MeshInfo.Meshes.size();
		for (int32_t MeshIndex = 0; MeshIndex < MeshSize; ++MeshIndex)
		{
			MeshDistanceInfo DisInfo;
			std::shared_ptr<MeshBase> Mesh = MeshInfo.Meshes[MeshIndex];

			math::Vector3 BoxPoint[8]{};
			Mesh->GetBoundingBox().GetPoint(BoxPoint);

			float distanceMin = 100000.f;
			float distanceMax = 0.f;
			DisInfo.WorldTransform = MeshInfo.WorldTransform;
			DisInfo.PrevWorldTransform = MeshInfo.PrevWorldTransform;
			DisInfo.Mesh = Mesh;

			for (int32_t PointIndex = 0; PointIndex < 8; PointIndex++)
			{
				math::Vector3 Point = BoxPoint[PointIndex];

				math::Vector4 Point4 = math::Vector4(Point.x, Point.y, Point.z, 1.0f);
				math::Vector4 TargetPoint = Point4 * Mesh->GetMeshMat() ;
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
			Result.push_back(DisInfo);
		}
	}

	void BasePassRender::ActualDraw(RenderCore::DynamicRHI* RHI,const std::vector<GltfSceneMeshInfo>& MeshInfos,
									std::shared_ptr<CameraComponent> Camera, bool IsPreDraw)
	{
		C_P(BasePassRender);

		for (const auto& MeshInfo : MeshInfos)
		{
			size_t MeshSize = MeshInfo.Meshes.size();
			std::vector<MeshDistanceInfo> RenderResult;
			GraphMeshByDistance(MeshInfo, Camera->GetCameraPos(), RenderResult);
			std::sort(RenderResult.begin(), RenderResult.end(), MeshDistanceInfo());

			for (const auto& RenderInfo : RenderResult)
			{
				std::shared_ptr<MeshBase> Mesh = RenderInfo.Mesh;
				auto Material = GetOrCreateRender(Mesh);
				if (!Mesh->GetMaterial()->IsTransparent())
				{
					DrawMesh(RHI,
							 Mesh, MeshInfo.WorldTransform, MeshInfo.PrevWorldTransform,
							 Material, Camera, IsPreDraw);
				}
			}
		}

		SortMesh(MeshInfos, Camera->GetCameraPos());
		for (const auto& SortItem : d->SortMesh)
		{
			std::shared_ptr<MeshBase> Mesh = SortItem.Mesh;
			auto Material = GetOrCreateRender(Mesh);
			if (Mesh->GetMaterial()->IsTransparent())
			{
				DrawMesh(RHI,
						 Mesh, SortItem.WorldTransform, SortItem.PrevWorldTransform,
						 Material, Camera, IsPreDraw);
			}
		}
	}

	void BasePassRender::DrawMesh(RenderCore::DynamicRHI* RHI,
		std::shared_ptr<MeshBase> Mesh, const math::Matrix4x4& WorldTransform,
		const math::Matrix4x4& PrevWorldTransform,
		std::shared_ptr<MaterialRender> Render,
		std::shared_ptr<CameraComponent> Camera, bool IsPreDraw)
	{
		C_P(BasePassRender);

		auto Scene = GEngine->GetSceneRender();

		MaterialRenderParam RenderParam;
		RenderParam.lightInfos = d->sceneView->GetLights();
		RenderParam.CameraPos = Camera->GetCameraPos();
		RenderParam.CurrModelMatrix = Mesh->GetMeshMat() * WorldTransform;
		RenderParam.PrevModelMatrix = Mesh->GetMeshMat() * PrevWorldTransform;
		if (Scene->GetPostProcessor()->GetPostProcessorAAType() == EPostProcessorAAType::TAA)
			RenderParam.CurrViewProjMatrix = Camera->GetViewMatrix() * Camera->HackAddTemporalAAProjectionJitter(false);
		else
			RenderParam.CurrViewProjMatrix = Camera->GetViewMatrix() * Camera->GetProjMatrix();
		RenderParam.CurrViewProjInverseMatrix = RenderParam.CurrViewProjMatrix.Inverse();
		if (Scene->GetPostProcessor()->GetPostProcessorAAType() == EPostProcessorAAType::TAA)
			RenderParam.PrevViewProjMatrix = Camera->GetPrevViewMatrix() * Camera->HackAddTemporalAAProjectionJitter(true);
		else
			RenderParam.PrevViewProjMatrix = Camera->GetPrevViewMatrix() * Camera->GetPrevProjMatrix();
		RenderParam.PrevViewProjInverseMatrix = RenderParam.PrevViewProjMatrix.Inverse();
		RenderParam.TemporalAAJitter = Camera->GetTemporalAAJitter();
		RenderParam.HasSkin = Mesh->HasSkin();
		RenderParam.preProcessor = Scene->GetPreProcessor();
		math::Matrix4x4 Rotate = math::Matrix4x4::RotateX(math::Radians(d->xHDRRotate));
		Rotate *= math::Matrix4x4::RotateY(math::Radians(d->yHDRRotate));
		RenderParam.RotateIBL = Rotate;
		RenderParam.TargetBuffer = d->TargetBuffer;

		if (!IsPreDraw && Mesh->GetSkinId() > -1 && Mesh->GetBoneNodeArray().size() > 0)
		{
			auto& Bone = Mesh->GetBoneNodeArray()[Mesh->GetSkinId()];
			for (uint32_t BoneIndex = 0; BoneIndex < Bone.size(); BoneIndex++)
			{
				Render->SetBoneMatrix(Bone[BoneIndex].FinalMat, BoneIndex);
			}
		}
		if (IsPreDraw)
		{
			Render->PreDraw(*RHI->GetDefaultCommandContext(), RenderParam);
		}
		else
		{
			Render->Draw(*RHI->GetDefaultCommandContext(), RenderParam);
		}

	}

	std::shared_ptr<Engine::MaterialRender> BasePassRender::GetOrCreateRender(std::shared_ptr<MeshBase> Mesh)
	{
		C_P(BasePassRender);
		
		auto ItFind = d->Renders.find(Mesh);
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
		{
			auto furMaterial = std::static_pointer_cast<GltfFurMaterial>(Mesh->GetMaterial());
			PBRMaterial = std::make_shared<FurMaterialRender>(Mesh->GetMeshBuffer(), furMaterial, furMaterial->GetFurConfig(), furMaterial->GetNoiseTex());
		}
			break;
		default:
			PBRMaterial = std::make_shared<PBRMaterialRender>(Mesh->GetMeshBuffer(), Mesh->GetMaterial());
		}

		PBRMaterial->InitRenderResource();
		d->Renders.insert({ Mesh,PBRMaterial });
		return PBRMaterial;
	}

}