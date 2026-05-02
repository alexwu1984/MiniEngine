#include "Scene/GltfActor.h"
#include "Scene/CameraComponent.h"
#include "Scene/SceneMeshComponent.h"
#include "Scene/GltfInputComponent.h"
#include "Scene/World.h"
#include "GltfModel/GltfModel.h"
#include "core/strings.h"

namespace Engine
{
	namespace
	{
		static void ApplyActorDisplayNameFromSceneJson(Actor& ActorRef, const nlohmann::json& GltfJson)
		{
			try
			{
				if (GltfJson.find("ActorName") != GltfJson.end() && GltfJson["ActorName"].is_string())
				{
					ActorRef.SetActorName(core::u8_ucs2(GltfJson["ActorName"].get<std::string>()));
					return;
				}
				if (GltfJson.find("Model") != GltfJson.end() && GltfJson["Model"].is_string())
				{
					std::string m = GltfJson["Model"].get<std::string>();
					const size_t slash = m.find_last_of("/\\");
					if (slash != std::string::npos)
						m = m.substr(slash + 1);
					const size_t dot = m.find_last_of('.');
					if (dot != std::string::npos)
						m = m.substr(0, dot);
					if (!m.empty())
						ActorRef.SetActorName(core::u8_ucs2(m));
					return;
				}
				if (GltfJson.find("ProceduralFloor") != GltfJson.end())
					ActorRef.SetActorName(L"ProceduralFloor");
			}
			catch (const std::exception&)
			{
			}
		}
	} // namespace

	IMP_ACTOR_CLASS_NAME(GltfActor)
	IMP_ACTOR_TRAITS_CLASS_NAME(GltfActor)

	struct GltfActorPrivate
	{
		nlohmann::json GltfJson;
		std::shared_ptr<CameraComponent> CameraComp;
		std::shared_ptr<SceneMeshComponent> MeshComp;
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
		ApplyActorDisplayNameFromSceneJson(*this, d->GltfJson);
		d->MeshComp = std::make_shared<SceneMeshComponent>(this->shared_from_this());
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
			d->MeshComp->SetProjectShadow(d->GltfJson["ProjShadow"]);
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
		{
			bool bMouseRotateModel = !GetWorld()->UsesRoamCameraScene();
			if (d->GltfJson.find("MouseRotateModel") != d->GltfJson.end() && !d->GltfJson["MouseRotateModel"].is_null())
				bMouseRotateModel = d->GltfJson["MouseRotateModel"].get<bool>();
			d->InputComp->SetMouseRotateModelEnabled(bMouseRotateModel);
		}
		AddComponent(d->InputComp);
	}

}