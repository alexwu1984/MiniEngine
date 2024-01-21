#include "Scene/GltfActor.h"
#include "Scene/OrbitCamera.h"
#include "Scene/GltfMeshComponent.h"
#include "Scene/GltfInputComponent.h"

namespace Engine
{
	IMP_ACTOR_CLASS_NAME(GltfActor)
	IMP_ACTOR_TRAITS_CLASS_NAME(GltfActor)

	struct GltfActorP
	{
		nlohmann::json GltfJson;
		std::shared_ptr<CameraComponent> CameraComp;
		std::shared_ptr<GltfMeshComponent> MeshComp;
		std::shared_ptr<GltfDeviceInputComponent> InputComp;
	};

	GltfActor::GltfActor(std::weak_ptr<SceneView> Scene, const nlohmann::json& GltfJson)
		: Actor(Scene)
		, Impl(std::make_shared<GltfActorP>())
	{
		Impl->GltfJson = GltfJson;
	}

	GltfActor::~GltfActor()
	{

	}

	void GltfActor::InitResouce()
	{
		Actor::InitResouce();

		Impl->MeshComp = std::make_shared<GltfMeshComponent>(this->shared_from_this());
		bool bLoad = Impl->MeshComp->Load(Impl->GltfJson);
		AddComponent(Impl->MeshComp);
		Impl->CameraComp = std::make_shared<CameraComponent>(this->shared_from_this());
		Impl->CameraComp->InitResource();
		AddComponent(Impl->CameraComp);

		Impl->InputComp = std::make_shared<GltfDeviceInputComponent>(this->shared_from_this());
		Impl->InputComp->InitResource();
		AddComponent(Impl->InputComp);

		if (bLoad)
		{
			auto Box = Impl->MeshComp->GetModelBox();
			math::Vector3 Length = Box.GetMaxPoint() - Box.GetMinPoint();
			float Dist = (std::max)(Length.x, (std::max)(Length.y, Length.z));
			Dist = Dist * 2.5;
			//m_Camera.SetDistanceToRO(z);
			auto Pos = Impl->CameraComp->GetCameraPos();
			Impl->CameraComp->SetCameraPos(math::Vector3(Pos.x,Pos.y, Dist));
		}

		
	}

}