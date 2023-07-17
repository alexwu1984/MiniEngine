#include "Scene/GltfMeshComponent.h"
#include "GltfModel/GltfModel.h"
#include "GltfModel/GltfMesh.h"
#include "GltfModel/GltfMaterial.h"
#include "GltfModel/GltfSkeleton.h"
#include "GltfModel/GltfModelConfig.h"
#include "Render/PBRMaterialRender.h"
#include "Render/FurMaterialRender.h"
#include "RHI/RHICommandContext.h"
#include "RHI/DynamicRHI.h"
#include "Scene/Actor.h"
#include "Scene/CameraComponent.h"
#include "Thread/RenderThread.h"
#include "core/logger.h"
#include "Engine/Engine.h"
#include "Scene/SceneView.h"

namespace Engine
{
	struct MeshDistanceInfo
	{
		float Distance;
		int32_t MeshID;
		int32_t ModelID;
		//区分mesh包围框的最近最远点
		int32_t PosType;
		

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
		float TotalDeltaTime = 0.f;
		std::shared_ptr< GltfModelConfig>  ModelConfig;
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
		std::filesystem::path Path = FileName;
		if (!Path.has_extension())
		{
			core::err() << __FUNCTION__ << " Load File failed:" << FileName;
			return false;
		}
		std::wstring Extension =  Path.extension().wstring();

		if (Extension == L".json")
		{
			Impl->ModelConfig = std::make_shared<GltfModelConfig>(std::static_pointer_cast<GltfMeshComponent>(this->shared_from_this()));
			if (Impl->ModelConfig->Load(FileName))
			{
				std::wstring Path = std::filesystem::path(FileName).parent_path().wstring();
				Path += L"/" + Impl->ModelConfig->GetModel();
				if (!Impl->Model.Load(Path, Impl->ModelConfig))
				{
					return false;
				}
			}
			else
			{
				return false;
			}
		}
		else
		{
			if (!Impl->Model.Load(FileName,nullptr))
			{
				return false;
			}
		}

		size_t MeshSize = Impl->Model.GetModelMesh().size();

		for (size_t MeshIndex = 0; MeshIndex < MeshSize; ++MeshIndex)
		{
			std::shared_ptr<GltfMesh> Mesh = Impl->Model.GetModelMesh()[MeshIndex];
			//default Material
			std::shared_ptr<PBRMaterialRender> PBRMaterial;

			switch (Mesh->GetMaterial()->GetMaterialType())
			{
			case GltfMaterial::MaterialType::PBR:
				PBRMaterial = std::make_shared<PBRMaterialRender>(Mesh->GetMeshBuffer(), Mesh->GetMaterial());
				break;
			case GltfMaterial::MaterialType::FUR:
				//PBRMaterial = std::make_shared<FurMaterialRender>(Mesh->GetMeshBuffer(), Mesh->GetMaterial());
				PBRMaterial = std::make_shared<PBRMaterialRender>(Mesh->GetMeshBuffer(), Mesh->GetMaterial());
				break;
			default:
				PBRMaterial = std::make_shared<PBRMaterialRender>(Mesh->GetMeshBuffer(), Mesh->GetMaterial());
			}

			PBRMaterial->InitRenderResource();
			Impl->Renders.insert({ MeshIndex,PBRMaterial });
		}

		return true;
	}

	GltfModel& GltfMeshComponent::GetModel() const
	{
		return Impl->Model;
	}

	math::AABB3 GltfMeshComponent::GetModelBox() const
	{
		return Impl->Model.GetModelBox();
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
			auto Material = Impl->Renders[MeshIndex];
			std::shared_ptr<GltfMesh> Mesh = Impl->Model.GetModelMesh()[MeshIndex];
			if (!Mesh->GetMaterial()->IsTransparent())
			{

				DrawMesh(Mesh, GetOwner()->GetWorldTransform(), Material, Camera,0);
			}
		}

		SortMesh(Camera->GetCameraPos());

		MeshSize = Impl->SortMesh.size();

		//draw transparent
		for (size_t MeshIndex = 0; MeshIndex < MeshSize; ++MeshIndex)
		{
			int MeshID = Impl->SortMesh[MeshIndex].MeshID;
			int ModelID = Impl->SortMesh[MeshIndex].ModelID;
			int ModelPosType = Impl->SortMesh[MeshIndex].PosType;
			std::shared_ptr<GltfMesh> Mesh = Impl->Model.GetModelMesh()[MeshID];
			if (Mesh->GetMaterial()->IsTransparent())
			{
				auto Material = Impl->Renders[MeshIndex];

				if (Mesh->GetSkinId() > -1 && Impl->Model.GetSkeleton()->GetBoneNodeArray().size() > 0)
				{
					auto& Bone = Impl->Model.GetSkeleton()->GetBoneNodeArray()[Mesh->GetSkinId()];
					for (uint32_t BoneIndex = 0; BoneIndex < Bone.size(); BoneIndex++)
					{
						Material->SetBoneMatrix(Bone[BoneIndex].FinalMat, BoneIndex);
					}
				}

				DrawMesh(Mesh, GetOwner()->GetWorldTransform(), Impl->Renders[MeshID], Camera, ModelPosType);
			}

		}

	}

	void GltfMeshComponent::Tick(float deltaTime)
	{
		Impl->TotalDeltaTime += deltaTime / 1000.f;
		auto RenderMesh = [Impl = Impl, deltaTime](RenderCore::DynamicRHI* DyRHI)
		{
			Impl->Model.Play(Impl->TotalDeltaTime, deltaTime);
		};

		ENQUEUE_UNIQUE_RENDER_COMMAND(RenderMesh);
	}

	void GltfMeshComponent::OnUpdateWorldTransform(float deltaTime)
	{
		auto UpdateTransform = [Impl = Impl,this, deltaTime](RenderCore::DynamicRHI* DyRHI)
		{
			auto& RootNodes = Impl->Model.GetSkeleton()->GetRootNode();
			if (!RootNodes.empty())
			{
				math::Matrix4x4 WorldTransform = GetOwner()->GetWorldTransform();
				RootNodes[0]->ParentMat = WorldTransform;
			}
			
		};

		ENQUEUE_UNIQUE_RENDER_COMMAND(UpdateTransform);
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

			float distanceMin = 100000.f;
			float distanceMax = 0.f;

			for (int32_t PointIndex = 0; PointIndex < 8; PointIndex++)
			{
				math::Vector3 Point = BoxPoint[PointIndex];

				math::Vector4 Point4 = math::Vector4(Point.x, Point.y, Point.z, 1.0f);
				math::Vector4 TargetPoint = Point4 * Mesh->GetMeshMat() * ModelMatrix;
				TargetPoint = TargetPoint / TargetPoint.w;

				////计算两个向量Z的距离
				//float Distance = math::Abs(TargetPoint.z - CameraPos.z);
				//DisInfo.Distance = Distance;
				//if (distanceMax < Distance)
				//{
				//	distanceMax = Distance;
				//	SortMesh[MeshIndex] = DisInfo;
				//}

				//计算两个向量Z的距离
				float Distance = math::Abs(TargetPoint.z - CameraPos.z);

				if (Distance < distanceMin)
				{
					distanceMin = Distance;

					DisInfo.PosType = 1;
					DisInfo.Distance = distanceMin;
					SortMesh[MeshIndex] = DisInfo;
				}
				if (Distance > distanceMax)
				{
					distanceMax = Distance;

					DisInfo.PosType = 0;
					DisInfo.Distance = distanceMax;
					SortMesh[MeshIndex] = DisInfo;
				}

			}
		}
		std::sort(Impl->SortMesh.begin(), Impl->SortMesh.end(), MeshDistanceInfo());
	}

	void GltfMeshComponent::DrawMesh(std::shared_ptr<GltfMesh> Mesh, const math::Matrix4x4& WorldTransform, 
		std::shared_ptr<MaterialRender> MaterialRender, std::shared_ptr<CameraComponent> Camera, int32_t PosType)
	{
		if (!MaterialRender)
		{
			return;
		}
		MaterialRenderParam RenderParam;
		RenderParam.CameraPos = Camera->GetCameraPos();
		RenderParam.CurrModelMatrix = Mesh->GetMeshMat() * WorldTransform;
		RenderParam.CurrViewProjMatrix = Camera->GetViewMatrix() * Camera->GetProjMatrix();
		RenderParam.CurrViewProjInverseMatrix = RenderParam.CurrViewProjMatrix.Inverse();
		RenderParam.PosType = PosType;
		RenderParam.HasSkin = Mesh->HasSkin();

		auto RenderMesh = [MaterialRender, RenderParam, Mesh ,Impl = Impl](RenderCore::DynamicRHI* DyRHI)
		{
			if (Mesh->GetSkinId() > -1 && Impl->Model.GetSkeleton()->GetBoneNodeArray().size() > 0)
			{
				auto& Bone = Impl->Model.GetSkeleton()->GetBoneNodeArray()[Mesh->GetSkinId()];
				for (uint32_t BoneIndex = 0; BoneIndex < Bone.size(); BoneIndex++)
				{
					MaterialRender->SetBoneMatrix(Bone[BoneIndex].FinalMat, BoneIndex);
				}
			}

			MaterialRender->Draw(*DyRHI->GetDefaultCommandContext(), RenderParam);

			GEngine->GetScene()->SetCanHandleInput(true);
		};

		ENQUEUE_UNIQUE_RENDER_COMMAND(RenderMesh);

	}

}
