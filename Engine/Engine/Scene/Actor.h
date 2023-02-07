#pragma once
#include "math/quaternion.h"
#include "math/matrix4x4.h"
#include "ActorTraits.h"
#include "Scene/DeviceInputState.h"

namespace Engine
{
	class SceneView;
	class Component;


#define DECLARE_ACTOR_CLASS_NAME(ClassName)\
	public:\
		static std::string Name;\
		virtual std::string GetName() {return Name;}

#define IMP_ACTOR_CLASS_NAME(ClassName)\
	std::string ClassName::Name = {#ClassName};

	struct ActorP;

	class Actor : public std::enable_shared_from_this<Actor>
	{
	public:
		enum AState : uint8_t
		{
			EActive,
			EPaused,
			EDead,
		};

		enum Flag : uint8_t
		{
			IsActor = true,
		};

	public:
		DECLARE_ACTOR_CLASS_NAME(Actor)
		Actor(std::weak_ptr<SceneView> Scene);
		virtual ~Actor();

		virtual void InitResouce();
		void Tick(float deltaTime);
		void TickComponents(float deltaTime);
		virtual void TickActor(float deltaTime);

		math::Vector3 GetPosition() const;
		void SetPosition(const math::Vector3& pos);
		float GetScale() const;
		void SetScale(float scale);
		math::Quaternion GetRotation() const;
		void SetRotation(const math::Quaternion& rotation);

		void ComputeWorldTransform();
		const math::Matrix4x4& GetWorldTransform() const;

		AState GetState() const;
		void SetState(AState state);

		std::shared_ptr<SceneView> GetScene() const;

		math::Vector3 GetForward() const;
		math::Vector3 GetRight() const;
		math::Vector3 GetUp() const;

		void RotateToNewForward(const math::Vector3& forward);

		void AddComponent(std::shared_ptr<Component> component);
		void RemoveComponent(std::shared_ptr<Component> component);

		std::vector<std::shared_ptr<Component>>& GetComponents() const;

	public:
		virtual void ProcessInput(const InputDeviceState& State);

	protected:
		std::shared_ptr<ActorP> GetActorP() const {return ImplActorP;}
	private:
		std::shared_ptr<ActorP> ImplActorP;
	};

	DECLARE_ACTOR_TRAITS_CLASS_NAME(Actor);

	template<typename TActorType>
	static __forceinline std::shared_ptr<TActorType> ActorCast(std::shared_ptr<Actor> Resource)
	{
		static_assert(TActorType::Flag::IsActor);

		if (Resource->GetName() == ActorTraitsClassName<TActorType>::Name)
		{
			return std::static_pointer_cast<TActorType>(Resource);
		}
		return nullptr;
	}
}
