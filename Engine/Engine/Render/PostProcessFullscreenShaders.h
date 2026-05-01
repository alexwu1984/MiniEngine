#pragma once
#include "core/inc.h"

namespace RenderCore
{
	class DynamicRHI;
	class RHIVertexShader;
}

namespace Engine
{
	class PostProcessFullscreenShaders
	{
	public:
		PostProcessFullscreenShaders(RenderCore::DynamicRHI* RHI);

		void InitResource();
		std::shared_ptr<RenderCore::RHIVertexShader> GetVertexShader() const;

	private:
		RenderCore::DynamicRHI* RHI = nullptr;
		std::shared_ptr<RenderCore::RHIVertexShader> VertexShader;
	};
}
