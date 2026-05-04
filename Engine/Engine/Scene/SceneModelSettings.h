#pragma once
#include "math/vector3.h"
#include "math/vector4.h"

namespace Engine
{
	// Fur/Hair runtime parameters authored in scene JSON.
	struct FurConfig
	{
		std::string Name;
		std::string NoiseTex;
		float FurLength{ 0.18f };
		float FurAmbientStrength{ 2.94f };
		float FurLevel{ 28.f };
		float UVScale{ 30.f };
		float FurLightExposure{ 0.4f };

		math::Vector3 Gravity{ 0.f,0.f,0.f };
		math::Vector3 FurColor{ 1.f,1.f,1.f };
	};

	// Simple material override parameters authored in scene JSON.
	struct MaterialConfig
	{
		bool UseConfig{ false };
		float Metallic{ 0.0 };
		float Roughness{ 1.0 };
		math::Vector4 BaseColor{ 1.0f,1.0f,1.0f,1.0f };
	};
}

