#include "win/win32.h"
#include "Scene.h"
#include "Sphere.h"
#include "Triangle.h"
#include "Light.h"
#include "Renderer.h"
#include "PointLight.h"
#include "DirectionLight.h"

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PWSTR pCmdLine, int nCmdShow)
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
	//scene.Add(std::make_unique<DirectionalLight>(math::Vector3(30, 50, -12), 0.5));
	scene.Add(std::make_unique<PointPoint>(math::Vector3(0, 5, 0), 0.5));

	Renderer r;
	r.Render(scene);

	return 0;
}
