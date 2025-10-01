#pragma once
#include "core/inc.h"

namespace RenderCore
{
	class RHICommandContext;
	class DynamicRHI;
	class RHITexture2D;
}

namespace Engine
{
	struct TemporallAAPrivate;
	class GBuffer;
	class CameraComponent;

	class TemporallAA
	{
	public:
		TemporallAA(RenderCore::DynamicRHI* RHI);
		~TemporallAA();

		void InitResource();
		void Draw(RenderCore::RHICommandContext& RHIContext, std::shared_ptr<GBuffer> TargetBuffer, std::shared_ptr<CameraComponent> Camera);
		std::shared_ptr<RenderCore::RHITexture2D> GetHistoryBuffer();
	private:
		TemporallAAPrivate* d_ptr = nullptr;
	};
}