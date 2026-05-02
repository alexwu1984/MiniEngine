#pragma once
#include "Object.h"
#include "light.h"

class Scene
{
public:
	// setting up options
	int width = 1280;
	int height = 960;
	double fov = 90;
	math::Vector3 backgroundColor = math::Vector3(0.235294, 0.67451, 0.843137);
	int maxDepth = 5;
	float epsilon = 0.00001;

	Scene(int w, int h) : width(w), height(h)
	{}

	void Add(std::unique_ptr<Object> object) { objects.push_back(std::move(object)); }
	void Add(std::unique_ptr<Light> light) { lights.push_back(std::move(light)); }

	const std::vector<std::unique_ptr<Object> >& get_objects() const { return objects; }
	const std::vector<std::unique_ptr<Light> >& get_lights() const { return lights; }

private:
	// creating the scene (adding objects and lights)
	std::vector<std::unique_ptr<Object> > objects;
	std::vector<std::unique_ptr<Light> > lights;
};