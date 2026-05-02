#pragma once
#include "math/quaternion.h"
#include "math/matrix4x4.h"
#include "ActorTraits.h"
#include "Scene/DeviceInputState.h"

namespace Engine
{
	class World;
	class Component;


#define DECLARE_ACTOR_CLASS_NAME(ClassName)\
	public:\
		static std::string Name;\
		virtual std::string GetName() {return Name;}

#define IMP_ACTOR_CLASS_NAME(ClassName)\
	std::string ClassName::Name = {#ClassName};

	struct ActorPrivate;

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
		Actor(std::weak_ptr<World> InWorld);
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

		void ComputeWorldTransform(float deltaTime);
		const math::Matrix4x4& GetWorldTransform() const;
		const math::Matrix4x4& GetPrevWorldTransform() const;

		AState GetState() const;
		void SetState(AState state);

		std::shared_ptr<World> GetWorld() const;

		math::Vector3 GetForward() const;
		math::Vector3 GetRight() const;
		math::Vector3 GetUp() const;

		void RotateToNewForward(const math::Vector3& forward);

		void AddComponent(std::shared_ptr<Component> component);
		void RemoveComponent(std::shared_ptr<Component> component);

		/** False if internal actor state is missing (should not happen for constructed actors). */
		bool IsActorPrivateAllocated() const noexcept;

		std::vector<std::shared_ptr<Component>>& GetAllComponents() const;
		template<typename TComponent> std::vector<std::shared_ptr<TComponent>> GetComponents() const;
		template<typename TComponent> std::shared_ptr<TComponent> GetComponent() const;

		void SetVisible(bool visible);
		bool IsVisible() const;

		void SetActorName(const std::wstring& name);
		std::wstring GetActorName() const;

		uint64_t GetStableInstanceId() const noexcept;

	public:
		virtual void ProcessInput(const InputDeviceState& State);

	protected:
		ActorPrivate* GetActorP() const { return d_ptr; }

	private:
		ActorPrivate* d_ptr = nullptr;
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


	template<typename TComponent>
	std::shared_ptr<TComponent>  Actor::GetComponent() const
	{
		const auto& Components = GetAllComponents();
		if (Components.empty())
		{
			return {};
		}

		for (const auto& Comp : Components)
		{
			auto Temp = ComponentCast<TComponent>(Comp);
			if (Temp)
			{
				return Temp;
			}
		}
		return {};
	}

	template<typename TComponent> 
	std::vector<std::shared_ptr<TComponent>> Actor::GetComponents() const
	{
		const auto& Components = GetAllComponents();
		if (Components.empty())
		{
			return {};
		}

		std::vector<std::shared_ptr<TComponent>> TempComps;
		for (const auto& Comp : Components)
		{
			auto Temp = ComponentCast<TComponent>(Comp);
			if (Temp)
			{
				TempComps.emplace_back(Temp);
			}
		}
		return TempComps;
	}
}
