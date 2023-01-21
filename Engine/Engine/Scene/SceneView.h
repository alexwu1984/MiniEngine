#pragma once
#include "core/inc.h"

namespace Engine 
{
	class Actor;
	
	struct SceneViewP;

	class SceneView : public std::enable_shared_from_this<SceneView>
	{
	public:
		SceneView();
		~SceneView();

		void Init();

		void AddActor(std::shared_ptr<Actor> actor);
		void RemoveActor(std::shared_ptr<Actor> actor);
		void Tick(float DeltaTime);

		template<typename ActorType>  std::vector<std::shared_ptr<ActorType>> GetActors()
		{
			std::vector<std::shared_ptr<ActorType>> Actors;
			for (auto ActorItem : AllActors())
			{
				std::shared_ptr<ActorType> convertActor = ActorCast<ActorType>(ActorItem);
				if (convertActor)
				{
					Actors.push_back(convertActor);
				}
			}
			return Actors;
		}
	private:
		std::vector<std::shared_ptr<Actor>>& AllActors();
	private:
		std::unique_ptr< SceneViewP> Impl;
	};

}

