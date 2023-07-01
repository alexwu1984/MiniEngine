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
		std::string NoiseTex;
		float FurLength{ 0.18 };
		float FurAmbientStrength{ 2.94 };
		float FurLevel{ 28 };
		float UVScale{ 30 };
		math::Vector3 Gravity{ 0,-1,0 };
	};

	class GltfModelConfig
	{
	public:
		GltfModelConfig(std::weak_ptr< GltfMeshComponent> Owner);
		~GltfModelConfig();

		bool Load(const std::wstring& FileName);
		std::wstring GetModel() const;

		const std::vector< DynamicBoneInfo>& GetDyNamicBoneInfoList() const;

	private:
		GltfModelConfigPrivate* d_ptr = nullptr;
	};
}