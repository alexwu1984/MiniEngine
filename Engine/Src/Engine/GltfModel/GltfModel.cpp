#include "GltfModel/GltfModel.h"
#include "tinygltf/tiny_gltf.h"

namespace Engine
{
	struct GltfModelP
	{
		tinygltf::TinyGLTF GltfCtx;
		tinygltf::Model GltfMode;
	};

	GltfModel::GltfModel()
		:Data(std::make_shared<GltfModelP>())
	{

	}

	GltfModel::~GltfModel()
	{

	}

}
