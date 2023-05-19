#pragma once
#include "core/inc.h"

namespace Engine
{
	struct TransformNodeP;

	class DyTransformNode
	{
	public:
		DyTransformNode(const char* id);
		~DyTransformNode();

		void AddChildNode(DyTransformNode* Child);
	private:
		TransformNodeP* Impl = nullptr;
	};
}

