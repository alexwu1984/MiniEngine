#pragma once
#include "core/inc.h"
#include "json.h"
#include "math/aabb3.h"

namespace Engine
{
	class MeshBase;

	struct ProceduralBuildResult
	{
		std::vector<std::shared_ptr<MeshBase>> Meshes;
		math::AABB3 Box;
	};

	// Build an XZ plane floor from json config (Finish: glossy | matte | frosted | grass).
	// Expected format:
	// {
	//   "Size": "60,60",
	//   "Segments": "200,200",
	//   "UVScale": 10.0,
	//   "Material": { "Metallic": 1.0, "Roughness": 0.85, "BaseColor": "0.65,0.68,0.72,1.0" }
	// }
	bool BuildProceduralFloor(const nlohmann::json& FloorJson, ProceduralBuildResult& OutResult);
}

