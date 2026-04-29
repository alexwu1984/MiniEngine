#include "Scene/GltfActor.h"
#include "Scene/OrbitCamera.h"
#include "Scene/GltfMeshComponent.h"
#include "Scene/GltfInputComponent.h"
#include "Scene/World.h"
#include "GltfModel/GltfModelConfig.h"
#include "GltfModel/GltfModel.h"

namespace Engine
{
	IMP_ACTOR_CLASS_NAME(GltfActor)
	IMP_ACTOR_TRAITS_CLASS_NAME(GltfActor)

	struct GltfActorPrivate
	{
		nlohmann::json GltfJson;
		std::shared_ptr<CameraComponent> CameraComp;
		std::shared_ptr<GltfMeshComponent> MeshComp;
		std::shared_ptr<GltfDeviceInputComponent> InputComp;
	};

	GltfActor::GltfActor(std::weak_ptr<World> InWorld, const nlohmann::json& GltfJson)
		: Actor(InWorld)
		, d_ptr(new GltfActorPrivate())
	{
		C_P(GltfActor);
		d->GltfJson = GltfJson;
	}

	GltfActor::~GltfActor()
	{
		delete d_ptr;
	}

	void GltfActor::InitResouce()
	{
		Actor::InitResouce();
		C_P(GltfActor);
		d->MeshComp = std::make_shared<GltfMeshComponent>(this->shared_from_this());
		bool bLoad = d->MeshComp->Load(d->GltfJson);
		if (!bLoad)
		{
			return;
		}
		//SetActorName(d->MeshComp->GetModel().GetModelConfig()->GetModelName());

		AddComponent(d->MeshComp);
		d->CameraComp = std::make_shared<CameraComponent>(this->shared_from_this());
		d->CameraComp->InitResource();

		if (d->GltfJson.find("MainCamera") != d->GltfJson.end())
		{
			if (d->GltfJson["MainCamera"])
			{
				GetWorld()->SetMainCamera(std::static_pointer_cast<CameraComponent>(d->CameraComp));
				auto Box = d->MeshComp->GetModelBox();
				math::Vector3 Length = Box.GetMaxPoint() - Box.GetMinPoint();
				float Dist = (std::max)(Length.x, (std::max)(Length.y, Length.z));
				Dist = Dist * 4;
				auto Pos = d->CameraComp->GetCameraPos();
				d->CameraComp->SetCameraPos(math::Vector3(Pos.x, Pos.y, Dist));
			}
		}

		if (d->GltfJson.find("Scale") != d->GltfJson.end())
		{
			SetScale(d->GltfJson["Scale"]);
		}

		if (d->GltfJson.find("ProjShadow") != d->GltfJson.end())
		{
			SetProjectShadow(d->GltfJson["ProjShadow"]);
		}

		if (d->GltfJson.find("Position") != d->GltfJson.end())
		{
			std::string PosStr = d->GltfJson["Position"];
			math::Vector3 Pos;
			std::sscanf(PosStr.c_str(), "%f,%f,%f", &Pos.x, &Pos.y, &Pos.z);
			SetPosition(Pos);
		}
		
		AddComponent(d->CameraComp);

		d->InputComp = std::make_shared<GltfDeviceInputComponent>(this->shared_from_this());
		d->InputComp->InitResource();
		AddComponent(d->InputComp);
	}

}