#pragma once
#include "math/aabb3.h"

namespace Engine
{
	class MeshBase;

	struct ProceduralModel
	{
		std::vector<std::shared_ptr<MeshBase>> Meshes;
		math::AABB3 Box;
	};
}

