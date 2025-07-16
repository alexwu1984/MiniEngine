#include "ViewerApp.h"
#include "Engine/Scene/GltfActor.h"
#include "Engine/Engine.h"
#include "Engine/Scene/SceneView.h"
#include "core/system.h"
#include "Scene/CameraComponent.h"
#include "App/AppWindow.h"
#include "Render/SceneRender.h"
#include "Render/MaterialPreFrame.h"
#include "Imgui/imgui.h"
#include "Tex2DRender.h"
#include "Thread/RenderThread.h"

#include "Scene.h"
#include "Sphere.h"
#include "Triangle.h"
#include "Light.h"
#include "PointLight.h"
#include "DirectionLight.h"

using namespace Engine;

ViewerApp::ViewerApp()
{
	
}

ViewerApp::~ViewerApp()
{

}

bool ViewerApp::Init()
{
	BuildRenderScene();
	//BuildExerciseScene();
	return true;
}

void ViewerApp::ShutDown()
{
	_Demo = {};
}

void ViewerApp::BuildRenderScene()
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

	math::Vector3 verts[4] = { {-5,-3,-6}, {5,-3,-6}, {5,-3,-16}, {-5,-3,-16} };
	uint32_t vertIndex[6] = { 0, 1, 3, 1, 2, 3 };
	math::Vector2 st[4] = { {0, 0}, {1, 0}, {1, 1}, {0, 1} };
	auto mesh = std::make_unique<MeshTriangle>(verts, vertIndex, 2, st);
	mesh->materialType = DIFFUSE_AND_GLOSSY;

	scene.Add(std::move(mesh));
	//scene.Add(std::make_unique<DirectionalLight>(math::Vector3(-20, 70, 20), 0.5));
	scene.Add(std::make_unique<DirectionalLight>(math::Vector3(30, 50, -12), 0.5));
	scene.Add(std::make_unique<PointPoint>(math::Vector3(-1, 4, -12), 20.f));

	m_render.Render(scene);
	core::vec2u size(scene.width, scene.height);

	ENQUEUE_UNIQUE_RENDER_COMMAND([this, size](RenderCore::DynamicRHI* RHI) {
		if (!_Demo)
		{
			_Demo = std::make_shared<Tex2DRender>(RHI);
		}
		_Demo->InitResource();
		_Demo->InitTex(size, m_render.GetBuffer());
		auto sceneRender = Engine::GEngine->GetSceneRender();
		sceneRender->SetSamplePostProcessor(_Demo);
	});
}

void ViewerApp::BuildExerciseScene()
{
	Scene scene(1280, 960);

	auto sph1 = std::make_unique<Sphere>(math::Vector3(0, 0, 0), 2);
	sph1->materialType = DIFFUSE_AND_GLOSSY;
	sph1->diffuseColor = math::Vector3(0.1, 0.1, 0.8);

	scene.Add(std::move(sph1));

	m_render.Exercise4(scene);
}
