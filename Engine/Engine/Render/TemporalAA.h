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
	class GBuffer;

	class TemporallAA
	{
	public:
		TemporallAA(RenderCore::DynamicRHI* RHI);
		~TemporallAA();

		void InitResource();
		void Draw(RenderCore::RHICommandContext& RHIContext, std::shared_ptr<GBuffer> TargetBuffer);
	private:
		TemporallAAPrivate* d_ptr = nullptr;
	};
}