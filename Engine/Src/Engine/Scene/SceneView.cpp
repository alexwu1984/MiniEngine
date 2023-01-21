#include "Scene/SceneView.h"
#include "Render/SceneRender.h"
#include "Scene/Actor.h"

namespace Engine 
{
	struct SceneViewP
	{
		std::unique_ptr <SceneRender> Render;
		std::vector<std::shared_ptr<Actor>> Actors;
		std::vector<std::shared_ptr<Actor>> PendingActors;

		bool UpdatingActors = false;
	};

	SceneView::SceneView()
		:Impl(std::make_unique<SceneViewP>())
	{

	}

	SceneView::~SceneView()
	{

	}

	void SceneView::Init()
	{
		Impl->Render = std::make_unique<SceneRender>(this->shared_from_this());
		Impl->Render->InitResource();
	}

	void SceneView::AddActor(std::shared_ptr<Actor> actor)
	{
		if (Impl->UpdatingActors)
		{
			Impl->PendingActors.emplace_back(actor);
		}
		else
		{
			Impl->Actors.emplace_back(actor);
		}
	}

	void SceneView::RemoveActor(std::shared_ptr<Actor> actor)
	{
		auto iter = std::find(Impl->PendingActors.begin(), Impl->PendingActors.end(), actor);
		if (iter != Impl->PendingActors.end())
		{
			// Swap to end of vector and pop off (avoid erase copies)
			std::iter_swap(iter, Impl->PendingActors.end() - 1);
			Impl->PendingActors.pop_back();
		}

		// Is it in actors?
		iter = std::find(Impl->Actors.begin(), Impl->Actors.end(), actor);
		if (iter != Impl->Actors.end())
		{
			// Swap to end of vector and pop off (avoid erase copies)
			std::iter_swap(iter, Impl->Actors.end() - 1);
			Impl->Actors.pop_back();
		}
	}

	void SceneView::Tick(float DeltaTime)
	{
		// Update all actors
		Impl->UpdatingActors = true;
		for (auto Item : Impl->Actors)
		{
			Item->Tick(DeltaTime);
			//Item->ProcessInput(m_InputSystem.GetState());
		}

		Impl->UpdatingActors = false;

		// Move any pending actors to mActors
		for (auto pending : Impl->PendingActors)
		{
			pending->Tick(DeltaTime);
			//pending->ProcessInput(m_InputSystem.GetState());
			Impl->Actors.emplace_back(pending);
		}
		Impl->PendingActors.clear();

		for (auto ItActor = Impl->Actors.begin(); ItActor != Impl->Actors.end();)
		{
			if ((*ItActor)->GetState() == Actor::EDead)
			{
				ItActor = Impl->Actors.erase(ItActor);
			}
			else
			{
				++ItActor;
			}
		}
	}

	template<typename ActorType> std::vector<std::shared_ptr<ActorType>>
	SceneView::GetActors()
	{
		std::vector<std::shared_ptr<ActorType>> Actors;
		for (auto Item : Impl->Actors)
		{
			std::shared_ptr<ActorType> convertActor = ActorCast<ActorType>(Item);
			if (convertActor)
			{
				Actors.push_back(convertActor);
			}
		}
		return Actors;
	}

}