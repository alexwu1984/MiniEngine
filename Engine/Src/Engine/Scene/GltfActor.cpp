#include "Scene/GltfActor.h"
#include "Scene/OrbitCamera.h"
#include "Scene/GltfMeshComponent.h"

namespace Engine
{
	IMP_ACTOR_CLASS_NAME(GltfActor)
	IMP_ACTOR_TRAITS_CLASS_NAME(GltfActor)

	struct GltfActorP
	{
		std::wstring ModelFileName;
		std::shared_ptr<OrbitCamera> CameraComp;
		std::shared_ptr<GltfMeshComponent> MeshComp;
	};

	GltfActor::GltfActor(std::weak_ptr<SceneView> Scene, const std::wstring& FileName)
		:Actor(Scene)
		, Impl(std::make_shared<GltfActorP>())
	{
		Impl->ModelFileName = FileName;
	}

	GltfActor::~GltfActor()
	{

	}

	void GltfActor::InitResouce()
	{
		Actor::InitResouce();

		Impl->MeshComp = std::make_shared<GltfMeshComponent>(this->shared_from_this());
		Impl->MeshComp->Load(Impl->ModelFileName);
		AddComponent(Impl->MeshComp);
		Impl->CameraComp = std::make_shared<OrbitCamera>(this->shared_from_this());
		Impl->CameraComp->InitResource();
		AddComponent(Impl->CameraComp);
	}

}