#pragma once
#include "core/inc.h"

namespace Engine
{
	struct GltfModelConfigPrivate;
	class GltfMeshComponent;
	struct DynamicBoneInfo;

	class GltfModelConfig
	{
	public:
		GltfModelConfig(std::weak_ptr< GltfMeshComponent> Owner);
		~GltfModelConfig();

		bool Load(const std::wstring& FileName);
		std::wstring GetModel() const;

	private:
		GltfModelConfigPrivate* Impl = nullptr;
	};
}