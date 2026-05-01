#pragma once
#include "core/inc.h"
#include "tinygltf/json.h"
#include "Render/FRDGBuilder.h"

namespace RenderCore
{
	class RHICommandContext;
	class DynamicRHI;
	class RHIViewPort;
	class RHITexture2D;
}

namespace Engine
{
	struct PostProcessorPrivate;
	struct FSceneViewData;
	class GBuffer;

	enum class EPostProcessorAAType : uint8_t
	{
		TAA,
		FXAA
	};

	class PostProcessor
	{
	public:
		PostProcessor(RenderCore::DynamicRHI* RHI);
		~PostProcessor();

		void LoadConfig(const nlohmann::json& Root);
		void InitResource();
		// After swapchain/GBuffer resolution change: drop intermediate targets only (no shader recompile).
		void InvalidateTransientResources();
		void Draw(RenderCore::RHICommandContext& RHIContext, std::shared_ptr<GBuffer> TargetBuffer, std::shared_ptr<RenderCore::RHIViewPort> ViewPort,
				  std::shared_ptr<const FSceneViewData> ViewData);
		void AddFramePasses(FRDGBuilder& Graph, RenderCore::RHICommandContext& RHIContext, std::shared_ptr<GBuffer> TargetBuffer,
							std::shared_ptr<RenderCore::RHIViewPort> ViewPort, std::shared_ptr<const FSceneViewData> ViewData);
		std::shared_ptr<RenderCore::RHITexture2D> GetSSRBuffer() const;
		EPostProcessorAAType GetPostProcessorAAType() const;

		/**
		 * When true, main-pass view-projection matrices should include sub-pixel Halton jitter (TAA path).
		 * Scene/view setup asks this once per frame; the actual jitter values still come from CameraComponent.
		 */
		bool WantsHaltonProjectionJitterForMainPass() const;

	private:
		void BuildSSRPasses(FRDGBuilder& Graph, RenderCore::RHICommandContext& RHIContext, std::shared_ptr<GBuffer> TargetBuffer,
							std::shared_ptr<RenderCore::RHIViewPort> ViewPort, std::shared_ptr<const FSceneViewData> ViewData,
							std::shared_ptr<RenderCore::RHITexture2D> SSRReflectionColor);
		void BuildBloomPasses(FRDGBuilder& Graph, RenderCore::RHICommandContext& RHIContext, std::shared_ptr<GBuffer> TargetBuffer,
							  std::shared_ptr<RenderCore::RHIViewPort> ViewPort, bool UseSSRComposite);
		void BuildAAPasses(FRDGBuilder& Graph, RenderCore::RHICommandContext& RHIContext, std::shared_ptr<GBuffer> TargetBuffer,
						   std::shared_ptr<RenderCore::RHIViewPort> ViewPort, std::shared_ptr<const FSceneViewData> ViewData,
						   std::shared_ptr<RenderCore::RHITexture2D> AntiAliasingColor);
		void BuildTonemappingPass(FRDGBuilder& Graph, RenderCore::RHICommandContext& RHIContext, std::shared_ptr<GBuffer> TargetBuffer,
								  std::shared_ptr<RenderCore::RHIViewPort> ViewPort);
	private:
		PostProcessorPrivate* d_ptr = nullptr;
	};
}