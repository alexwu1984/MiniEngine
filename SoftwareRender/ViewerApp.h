#pragma once
#include "core/inc.h"
#include "core/color.h"
#include "math/vector3.h"
#include "Renderer.h"

namespace RenderCore
{
	class DynamicRHI;
	class RHICommandContext;
	class RHIViewPort;
}

namespace Engine
{
	class AppWindow;
}

class Tex2DRender;

/** CPU ray trace + GPU blit viewer (standalone runner; no MainEngine / FWorldSceneRender). */
class ViewerApp
{
public:
	ViewerApp();
	~ViewerApp();

	void BuildCpuRayTraceScene();
	void BuildExerciseScene();

	void GpuInit(RenderCore::DynamicRHI* rhi);
	void GpuDraw(RenderCore::RHICommandContext& ctx, std::shared_ptr<RenderCore::RHIViewPort> viewport,
				 const std::shared_ptr<Engine::AppWindow>& window, float deltaSeconds);

	core::FLinearColor GetClearColor() const { return clearColor_; }

private:
	core::vec2u cpuTexSize_{};
	core::FLinearColor clearColor_{ 0.15f, 0.15f, 0.2f, 1.f };
	std::shared_ptr<Tex2DRender> demo_;
	Renderer renderer_;
};
