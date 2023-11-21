#pragma once
#include "core/inc.h"

namespace RenderCore
{
	class RHICommandContext;
	class DynamicRHI;
}


namespace Engine
{
	struct TemporallAAPrivate;

	class TemporallAA
	{
	public:
		TemporallAA(RenderCore::DynamicRHI* RHI);
		~TemporallAA();

		void InitResource();

	private:
		TemporallAAPrivate* d_ptr = nullptr;
	};
}