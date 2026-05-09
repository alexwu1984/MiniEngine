#pragma once
#include "core/inc.h"
#include "Scene/SceneModelSettings.h"
#include "Render/RenderStableIds.h"

namespace RenderCore
{
	class RHITexture2D;
}

namespace Engine
{
	class MaterialBase
	{
	public:
		enum class MaterialType : uint8_t
		{
			PBR,
			FUR,
		};
		MaterialBase() noexcept : StableMaterialInstanceId(AllocateMaterialStableInstanceId()) {}
		virtual ~MaterialBase() = default;

		uint64_t GetStableMaterialInstanceId() const noexcept { return StableMaterialInstanceId; }

		virtual MaterialType GetMaterialType() const = 0;
		virtual std::string GetMaterialName() const = 0;
		virtual bool IsTransparent() const = 0;
		/**
		 * UE-style translucent depth policy (see BasePass translucent): BLEND materials that use a base-color texture
		 * for opacity typically write scene depth so deferred passes recover the correct surface Z (shadows, lighting).
		 * Constant-alpha translucency keeps depth read-only (depth test, no write). Only consulted when IsTransparent().
		 */
		virtual bool WritesTranslucentDepthToSceneBuffer() const { return false; }
		/** glTF alphaMode MASK: fragment test in base pass; false for OPAQUE / BLEND. */
		virtual bool UsesMaterialAlphaMask() const { return false; }
		/** glTF material.alphaCutoff (default 0.5); only read when UsesMaterialAlphaMask(). */
		virtual float GetMaterialAlphaCutoff() const { return 0.5f; }
		virtual std::shared_ptr<RenderCore::RHITexture2D> GetBaseColorTexture() const = 0;
		virtual std::shared_ptr<RenderCore::RHITexture2D> GetMetallicRoughnessTexture() const = 0;
		virtual std::shared_ptr<RenderCore::RHITexture2D> GetNormalTexture() const = 0;
		virtual std::shared_ptr<RenderCore::RHITexture2D> GetEmissiveTexture() const = 0;
		virtual std::shared_ptr<RenderCore::RHITexture2D> GetOcclusionTexture() const = 0;
		virtual const MaterialConfig& GetMaterialConfig() const = 0;

	private:
		uint64_t StableMaterialInstanceId = 0;
	};
}