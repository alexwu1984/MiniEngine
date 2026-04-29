#pragma once
#include "win/win32.h"
#include "core/color.h"
#include "core/vec2.h"

namespace RenderCore
{
	class RHITexture2D;

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
		/** Legacy no-op hook (was DXGI waitable swapchain pacing). D3D12 uses Present-time frame fence instead. */
		virtual void WaitFrameLatency() {}
		virtual void Prepare() = 0;
		virtual void Clear(const core::FLinearColor& Color) {};
		virtual void Present() {};
		/**
		 * After ImGui::NewFrame (see Prepare()) and application UI have been emitted, encodes ImGui draw lists
		 * into the active RHI command list (ImGui::Render + backend RenderDrawData). No-op when ImGui is disabled.
		 */
		virtual void RHIImGuiRenderDrawData() {}
		/**
		 * Single entry for GPU submission through the immediate context(s) and swap-chain Present.
		 * When ImGui is used, call RHIImGuiRenderDrawData() earlier in the frame graph before this.
		 * Default is no-op; D3D12/D3D11 override. Intended to stay on the same thread as frame recording until a dedicated RHI thread exists.
		 */
		virtual void RHISubmitAndPresentFrame() {}
		virtual void Resize(uint32_t InSizeX, uint32_t InSizeY, bool bInIsFullscreen) = 0;
		virtual core::vec2u GetSize() const = 0;
		virtual std::shared_ptr<RHITexture2D> GetBackBuffer() const = 0;
	};
}