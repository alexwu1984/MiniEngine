#pragma once
#include "win/win32.h"
#include "core/color.h"
#include "core/vec2.h"

namespace RenderCore
{
	class RHIViewPort
	{
	public:
		RHIViewPort() = default;
		virtual ~RHIViewPort();

		/**
	 * Returns access to the platform-specific native resource pointer.  This is designed to be used to provide plugins with access
	 * to the underlying resource and should be used very carefully or not at all.
	 *
	 * @return	The pointer to the native resource or NULL if it not initialized or not supported for this resource type for some reason
	 */
		virtual void* GetNativeSwapChain() const { return nullptr; }
		/**
		 * Returns access to the platform-specific native resource pointer to a backbuffer texture.  This is designed to be used to provide plugins with access
		 * to the underlying resource and should be used very carefully or not at all.
		 *
		 * @return	The pointer to the native resource or NULL if it not initialized or not supported for this resource type for some reason
		 */
		virtual void* GetNativeBackBufferTexture() const { return nullptr; }
		/**
		 * Returns access to the platform-specific native resource pointer to a backbuffer rendertarget. This is designed to be used to provide plugins with access
		 * to the underlying resource and should be used very carefully or not at all.
		 *
		 * @return	The pointer to the native resource or NULL if it not initialized or not supported for this resource type for some reason
		 */
		virtual void* GetNativeBackBufferRT() const { return nullptr; }

		/**
		 * Returns access to the platform-specific native window. This is designed to be used to provide plugins with access
		 * to the underlying resource and should be used very carefully or not at all.
		 *
		 * @return	The pointer to the native resource or NULL if it not initialized or not supported for this resource type for some reason.
		 * AddParam could represent any additional platform-specific data (could be null).
		 */
		virtual void* GetNativeWindow(void** AddParam = nullptr) const { return nullptr; }

		virtual void SetRenderTarget() {};
		virtual void Prepare() = 0;
		virtual void Clear(const core::FLinearColor& Color) {};
		virtual void Present() {};
		virtual void Resize(uint32_t InSizeX, uint32_t InSizeY, bool bInIsFullscreen) = 0;
		virtual core::vec2u GetSize() const = 0;
	};
}