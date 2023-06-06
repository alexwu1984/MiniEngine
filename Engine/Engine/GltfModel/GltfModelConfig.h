#pragma once
#include "core/inc.h"

namespace Engine
{
	struct GltfModelConfigPrivate;
	class GltfMeshComponent;

	class GltfModelConfig
	{
	public:
		GltfModelConfig(std::weak_ptr< GltfMeshComponent> Owner);
		~GltfModelConfig();

		bool Load(const std::wstring& FileName);

	private:
		GltfModelConfigPrivate* Impl = nullptr;
	};
}