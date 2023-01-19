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


	template<typename TActorType> std::shared_ptr<TActorType> ActorCast(std::shared_ptr<Actor> Resource);
}
