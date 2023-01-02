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

		bool Load(const std::wstring& FileName);
	private:
		void LoadNode();
		void LoadMesh();
	private:
		std::shared_ptr< GltfModelP> Data;
	};
}