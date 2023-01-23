#include "Scene/GltfOrbitActor.h"
#include "Scene/OrbitCamera.h"
#include "Scene/GltfMeshComponent.h"

namespace Engine
{
	struct GltfOrbitActorP
	{
		std::wstring ModelFileName;
		std::shared_ptr<OrbitCamera> CameraComp;
		std::shared_ptr<GltfMeshComponent> MeshComp;
	};

	GltfOrbitActor::GltfOrbitActor(std::weak_ptr<SceneView> Scene, const std::wstring& FileName)
		:Actor(Scene)
		, Impl(std::make_shared<GltfOrbitActorP>())
	{
		Impl->ModelFileName = FileName;
	}

	GltfOrbitActor::~GltfOrbitActor()
	{

	}

	void GltfOrbitActor::InitResouce()
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