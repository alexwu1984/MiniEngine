#pragma once
#include "core/inc.h"
#include "Scene/SceneModelSettings.h"
#include "json.h"

namespace Engine
{
	struct SceneModelAssetPrivate;
	struct FDynamicBoneInfo;

	// UE-style asset: immutable parsed data from scene JSON model entry.
	// Shared across components/instances; does not reference any owning component.
	class SceneModelAsset
	{
	public:
		SceneModelAsset();
		~SceneModelAsset();

		bool Load(const nlohmann::json& ModelJson);
		std::wstring GetModelRelativePath() const;

		const std::vector< FDynamicBoneInfo>& GetDyNamicBoneInfoList() const;
		const FurConfig& GetFurConfig() const;
		const MaterialConfig& GetMaterialConfig() const;

	private:
		SceneModelAssetPrivate* d_ptr = nullptr;
	};
}