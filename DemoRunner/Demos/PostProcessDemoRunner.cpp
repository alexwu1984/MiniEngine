#include "DemoRunner/Demos/PostProcessDemoRunner.h"

#include "DemoRunner/Demos/PostProcessDemo.h"
#include "Imgui/imgui.h"

using namespace DemoRunner;

PostProcessDemoRunner::PostProcessDemoRunner() = default;
PostProcessDemoRunner::~PostProcessDemoRunner() = default;

void PostProcessDemoRunner::Init(RenderCore::DynamicRHI* InRHI,
								 const std::shared_ptr<RenderCore::RHIViewPort>&,
								 const std::shared_ptr<Engine::AppWindow>&)
{
	RHI = InRHI;
	Demo = std::make_unique<PostProcessorDemo>(RHI);
	Demo->InitResource();
}

void PostProcessDemoRunner::OnGui()
{
	ImGui::Text("PostProcessorDemo (fullscreen shader)");
}

void PostProcessDemoRunner::Draw(RenderCore::RHICommandContext& Ctx,
								 const std::shared_ptr<RenderCore::RHIViewPort>& ViewPort,
								 float DeltaTime)
{
	if (!Demo)
		return;
	Demo->Draw(Ctx, ViewPort, DeltaTime);
}

