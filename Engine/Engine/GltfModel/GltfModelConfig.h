#pragma once
#include "core/inc.h"
#include "math/vector3.h"

namespace Engine
{
	struct GltfModelConfigPrivate;
	class GltfMeshComponent;
	struct DynamicBoneInfo;

	struct GltfFurConfig
	{
		std::string Name;
		std::string NoiseTex;
		float FurLength{ 0.18 };
		float FurAmbientStrength{ 2.94 };
		float FurLevel{ 28 };
		float UVScale{ 30 };
		float FurLightExposure{ 0.4 };

		math::Vector3 Gravity{ 0,0,0 };
		math::Vector3 FurColor{ 1,1,1 };
	};

	class GltfModelConfig
	{
	public:
		GltfModelConfig(std::weak_ptr< GltfMeshComponent> Owner);
		~GltfModelConfig();

		bool Load(const std::wstring& FileName);
		std::wstring GetModel() const;

		const std::vector< DynamicBoneInfo>& GetDyNamicBoneInfoList() const;
		const GltfFurConfig& GetFurConfig() const;

	private:
		GltfModelConfigPrivate* d_ptr = nullptr;
	};
}