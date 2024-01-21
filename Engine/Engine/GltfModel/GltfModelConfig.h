#pragma once
#include "core/inc.h"
#include "math/vector3.h"
#include "json.h"

namespace Engine
{
	struct GltfModelConfigPrivate;
	class GltfMeshComponent;
	struct DynamicBoneInfo;

	struct GltfFurConfig
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

	class GltfModelConfig
	{
	public:
		GltfModelConfig(std::weak_ptr< GltfMeshComponent> Owner);
		~GltfModelConfig();

		bool Load(const std::wstring& FileName);
		bool Load(const nlohmann::json& GltfJson);
		std::wstring GetModel() const;

		const std::vector< DynamicBoneInfo>& GetDyNamicBoneInfoList() const;
		const GltfFurConfig& GetFurConfig() const;

	private:
		GltfModelConfigPrivate* d_ptr = nullptr;
	};
}