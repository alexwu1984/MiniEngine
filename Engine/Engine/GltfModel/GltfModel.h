#pragma once
#include "inc.h"

namespace Engine
{
	struct GltfModelP;

	class GltfModel
	{
	public:
		GltfModel();
		~GltfModel();

	private:
		std::shared_ptr< GltfModelP> Data;
	};
}