#pragma once
#include "core/inc.h"

namespace Engine { class AppWindow; }
namespace RenderCore
{
	class DynamicRHI;
	class RHICommandContext;
	class RHIViewPort;
}

namespace DemoRunner
{
	struct ClearColor
	{
		float r = 0.0f, g = 0.0f, b = 0.0f, a = 1.0f;
	};

	class IDemo
	{
	public:
		virtual ~IDemo() = default;
		virtual const char* GetName() const = 0;
		virtual ClearColor GetClearColor() const { return {}; }

		virtual void Init(RenderCore::DynamicRHI* RHI,
						  const std::shared_ptr<RenderCore::RHIViewPort>& ViewPort,
						  const std::shared_ptr<Engine::AppWindow>& Window) = 0;

		virtual void OnGui() {}

		virtual void Draw(RenderCore::RHICommandContext& Ctx,
						  const std::shared_ptr<RenderCore::RHIViewPort>& ViewPort,
						  float DeltaTime) = 0;
	};
}

