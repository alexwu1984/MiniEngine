#pragma once
#include "core/inc.h"
#include "Scene/DeviceInputState.h"

namespace Engine 
{
	class Actor;
	class CameraComponent;
	struct SceneViewP;

	class SceneView : public std::enable_shared_from_this<SceneView>
	{
	public:
		SceneView();
		~SceneView();

		void Init();

		void LoadScene(const std::wstring& ModelFile);
		void AddActor(std::shared_ptr<Actor> actor);
		void RemoveActor(std::shared_ptr<Actor> actor);
		void Tick(float DeltaTime);

		template<typename ActorType>  std::vector<std::shared_ptr<ActorType>> GetActors()
		{
			std::vector<std::shared_ptr<ActorType>> Actors;
			for (auto ActorItem : GetAllActors())
			{
				std::shared_ptr<ActorType> ConvertActor = ActorCast<ActorType>(ActorItem);
				if (ConvertActor)
				{
					Actors.push_back(ConvertActor);
				}
			}
			return Actors;
		}
		void SetMainCamera(std::shared_ptr<CameraComponent> Camera);
		std::shared_ptr<CameraComponent> GetMainCamera() const;
		std::vector<std::shared_ptr<Actor>>& GetAllActors() const;
	
	private:
		void OnMouseButtonDown(MouseButton Button, core::vec2f Pos);
		void OnMouseButtonUp(MouseButton Button, core::vec2f Pos);
		void OnMouseMove(MouseButton Button, core::vec2f Pos);
		void HandleMouseEvent(MouseEventType EventType, MouseButton Button, core::vec2f Pos);
		void OnMouseWheel(int32_t WheelValue);
	private:
		std::unique_ptr< SceneViewP> Impl;
	};

}

