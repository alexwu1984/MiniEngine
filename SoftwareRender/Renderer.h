#pragma once
#include "math/vector2.h"
#include "core/color.h"
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
	void Exercise4(const Scene& scene);
	uint8_t* GetBuffer();
private:
	std::vector<core::FColor> m_frameBuffer;
};