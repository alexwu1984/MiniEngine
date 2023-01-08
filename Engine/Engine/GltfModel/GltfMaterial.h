#pragma once
#include "core/inc.h"
#include "tinygltf/tiny_gltf.h"


namespace Engine
{
	struct GltfMaterialP;

	class GltfMaterial
	{

	public:
		GltfMaterial(tinygltf::Model* Model);
		~GltfMaterial();

		void  InitMaterial(uint32_t MaterialIndex);
		std::string GetMaterialName() const;
		bool IsTransparent() const;

	private:
		std::shared_ptr< GltfMaterialP> Data;
	};
}