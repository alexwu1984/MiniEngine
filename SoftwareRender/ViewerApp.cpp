#include "ViewerApp.h"
#include "core/system.h"
#include "RHI/DynamicRHI.h"
#include "RHI/RHICommandContext.h"
#include "RHI/RHIViewPort.h"
#include "App/AppWindow.h"
#include "Tex2DRender.h"

#include "Scene.h"
#include "Sphere.h"
#include "Triangle.h"
#include "Light.h"
#include "PointLight.h"
#include "DirectionLight.h"

ViewerApp::ViewerApp() = default;

ViewerApp::~ViewerApp() = default;

void ViewerApp::BuildCpuRayTraceScene()
{
	Scene scene(1280, 960);

	auto sph1 = std::make_unique<Sphere>(math::Vector3(-1, 0, -12), 2);
	sph1->materialType = DIFFUSE_AND_GLOSSY;
	sph1->diffuseColor = math::Vector3(0.1, 0.1, 0.8);

	auto sph2 = std::make_unique<Sphere>(math::Vector3(0.5, -0.5, -8), 1.5);
	sph2->ior = 1.5;
	sph2->materialType = REFLECTION_AND_REFRACTION;

	scene.Add(std::move(sph1));
	scene.Add(std::move(sph2));

	math::Vector3 verts[4] = { { -5, -3, -6 }, { 5, -3, -6 }, { 5, -3, -16 }, { -5, -3, -16 } };
	uint32_t vertIndex[6] = { 0, 1, 3, 1, 2, 3 };
	math::Vector2 st[4] = { { 0, 0 }, { 1, 0 }, { 1, 1 }, { 0, 1 } };
	auto mesh = std::make_unique<MeshTriangle>(verts, vertIndex, 2, st);
	mesh->materialType = DIFFUSE_AND_GLOSSY;

	scene.Add(std::move(mesh));
	scene.Add(std::make_unique<DirectionalLight>(math::Vector3(30, 50, -12), 0.5));
	scene.Add(std::make_unique<PointPoint>(math::Vector3(-1, 4, -12), 20.f));

	renderer_.Render(scene);
	cpuTexSize_ = core::vec2u(scene.width, scene.height);
}

void ViewerApp::BuildExerciseScene()
{
	Scene scene(1280, 960);

	auto sph1 = std::make_unique<Sphere>(math::Vector3(0, 0, 0), 2);
	sph1->materialType = DIFFUSE_AND_GLOSSY;
	sph1->diffuseColor = math::Vector3(0.1, 0.1, 0.8);

	scene.Add(std::move(sph1));

	renderer_.Exercise4(scene);
}

void ViewerApp::GpuInit(RenderCore::DynamicRHI* rhi)
{
	if (!rhi || cpuTexSize_.cx == 0 || cpuTexSize_.cy == 0)
		return;
	demo_ = std::make_shared<Tex2DRender>(rhi);
	demo_->InitResource();
	demo_->InitTex(cpuTexSize_, renderer_.GetBuffer());
}

void ViewerApp::GpuDraw(RenderCore::RHICommandContext& ctx, std::shared_ptr<RenderCore::RHIViewPort> viewport,
					   const std::shared_ptr<Engine::AppWindow>& window, float deltaSeconds)
{
	if (!demo_ || !viewport || !window)
		return;
	const int w = window->GetWidth();
	const int h = window->GetHeight();
	demo_->Draw(ctx, viewport, deltaSeconds, w, h);
}
