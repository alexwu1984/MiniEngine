#pragma once
#include "core/inc.h"

namespace Engine 
{
	struct SceneViewP;
	class Actor;

	class SceneView : public std::enable_shared_from_this<SceneView>
	{
	public:
		SceneView();
		~SceneView();

		void Init();

		void AddActor(std::shared_ptr<Actor> actor);
		void RemoveActor(std::shared_ptr<Actor> actor);
		void Tick(float DeltaTime);

		template<typename ActorType> std::vector<std::shared_ptr<ActorType>> GetActors();
	private:
		std::unique_ptr< SceneViewP> Impl;
	};


}