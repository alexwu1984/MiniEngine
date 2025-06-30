#pragma once
#include "math/vector2.h"

class Object;
class Scene;

struct hit_payload
{
	float tNear;
	uint32_t index;
	math::Vector2 uv;
	Object* hit_obj;
};

class Renderer
{
public:
	void Render(const Scene& scene);
private:
};