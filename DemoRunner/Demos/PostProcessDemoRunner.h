#pragma once

#include "DemoRunner/Demos/IDemo.h"

#include <memory>

class PostProcessorDemo;

namespace DemoRunner
{
	class PostProcessDemoRunner final : public IDemo
	{
	public:
		explicit PostProcessDemoRunner();
		~PostProcessDemoRunner() override;

		const char* GetName() const override { return "post"; }

		void Init(RenderCore::DynamicRHI* RHI,
				  const std::shared_ptr<RenderCore::RHIViewPort>& ViewPort,
				  const std::shared_ptr<Engine::AppWindow>& Window) override;

		void OnGui() override;

		void Draw(RenderCore::RHICommandContext& Ctx,
				  const std::shared_ptr<RenderCore::RHIViewPort>& ViewPort,
				  float DeltaTime) override;

	private:
		std::unique_ptr<PostProcessorDemo> Demo;
		RenderCore::DynamicRHI* RHI = nullptr;
	};
}

