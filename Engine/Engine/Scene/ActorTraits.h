#pragma once
#include "core/inc.h"

namespace Engine
{
	class Actor;

	template<typename TActorType>
	struct ActorTraitsClassName
	{

	};

	#define  DECLARE_ACTOR_TRAITS_CLASS_NAME(ClassName)\
	template<> struct ActorTraitsClassName<ClassName>\
	{\
		static std::string Name;\
	};\

	#define IMP_ACTOR_TRAITS_CLASS_NAME(ClassName)\
	std::string ActorTraitsClassName<ClassName>::Name={#ClassName};

	class Component;

	template<typename TComponentType>
	struct ComponentTraitsClassName
	{

	};

	#define  DECLARE_COMPONENT_TRAITS_CLASS_NAME(ClassName)\
	template<> struct ComponentTraitsClassName<ClassName>\
	{\
		static std::string Name;\
	};\

	#define IMP_COMPONENT_TRAITS_CLASS_NAME(ClassName)\
	std::string ComponentTraitsClassName<ClassName>::Name={#ClassName};

}
