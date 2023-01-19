#pragma once
#include "math/quaternion.h"
#include "math/matrix4x4.h"
#include "ActorTraits.h"

namespace Engine
{
	class Scene;
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
		enum State : uint8_t
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
		Actor(std::weak_ptr<Scene> world);
		virtual ~Actor();

		virtual void InitResouce();
		void Update(float deltaTime);
		void UpdateComponents(float deltaTime);
		virtual void UpdateActor(float deltaTime);

		math::Vector3 GetPosition() const;
		void SetPosition(const math::Vector3& pos);
		float GetScale() const;
		void SetScale(float scale);
		math::Quaternion GetRotation() const;
		void SetRotation(const math::Quaternion& rotation);
	protected:
		std::shared_ptr<ActorP> GetActorP() {return ImplActorP;}
	private:
		std::shared_ptr<ActorP> ImplActorP;
	};
}
