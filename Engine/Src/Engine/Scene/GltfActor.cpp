#include "Scene/GltfActor.h"
#include "Scene/GltfMeshComponent.h"
#include "Scene/SceneView.h"

namespace Engine
{
	struct GltfActorP
	{
		std::wstring FileName;
		std::shared_ptr<GltfMeshComponent> MeshComp;
	};

	GltfActor::GltfActor(std::weak_ptr<SceneView> Scene, const std::wstring& FileName)
		:Actor(Scene)
		,Impl(std::make_shared<GltfActorP>())
	{
		Impl->FileName = FileName;
	}

	GltfActor::~GltfActor()
	{

	}


	void GltfActor::InitResouce()
	{
		Actor::InitResouce();

		Impl->MeshComp = std::make_shared<GltfMeshComponent>(this->shared_from_this());
		bool Ret = Impl->MeshComp->Load(Impl->FileName);
		Assert(Ret);
		AddComponent(Impl->MeshComp);
		
		if (GetScene())
		{
			GetScene()->AddActor(this->shared_from_this());
		}
	}

}