#pragma once
#include "core/inc.h"

namespace RenderCore
{
	class RHITexture2D;
}

namespace Engine
{
	struct MaterialConfig;
	class MaterialBase
	{
	public:
		enum class MaterialType : uint8_t
		{
			PBR,
			FUR,
		};
		MaterialBase() = default;
		virtual ~MaterialBase() = default;

		virtual MaterialType GetMaterialType() const = 0;
		virtual std::string GetMaterialName() const = 0;
		virtual bool IsTransparent() const = 0;
		virtual std::shared_ptr<RenderCore::RHITexture2D> GetBaseColorTexture() const = 0;
		virtual std::shared_ptr<RenderCore::RHITexture2D> GetMetallicRoughnessTexture() const = 0;
		virtual std::shared_ptr<RenderCore::RHITexture2D> GetNormalTexture() const = 0;
		virtual std::shared_ptr<RenderCore::RHITexture2D> GetEmissiveTexture() const = 0;
		virtual std::shared_ptr<RenderCore::RHITexture2D> GetOcclusionTexture() const = 0;
		virtual const MaterialConfig& GetMaterialConfig() const = 0;
	};
}