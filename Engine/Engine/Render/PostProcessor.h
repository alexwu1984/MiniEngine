#pragma once
#include "core/inc.h"
#include "tinygltf/json.h"

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
	class PostProcessGraph;
	class GBuffer;
	class CameraComponent;

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
		void Draw(RenderCore::RHICommandContext& RHIContext, std::shared_ptr<GBuffer> TargetBuffer, 
				  std::shared_ptr<RenderCore::RHIViewPort> ViewPort, std::shared_ptr<CameraComponent> Camera);
		std::shared_ptr<RenderCore::RHITexture2D> GetSSRBuffer() const;
		EPostProcessorAAType GetPostProcessorAAType() const;
	private:
		void BuildSSRPasses(PostProcessGraph& Graph, RenderCore::RHICommandContext& RHIContext, std::shared_ptr<GBuffer> TargetBuffer,
							std::shared_ptr<RenderCore::RHIViewPort> ViewPort, std::shared_ptr<CameraComponent> Camera,
							std::shared_ptr<RenderCore::RHITexture2D> SSRReflectionColor);
		void BuildBloomPasses(PostProcessGraph& Graph, RenderCore::RHICommandContext& RHIContext, std::shared_ptr<GBuffer> TargetBuffer,
							  std::shared_ptr<RenderCore::RHIViewPort> ViewPort, bool UseSSRComposite);
		void BuildAAPasses(PostProcessGraph& Graph, RenderCore::RHICommandContext& RHIContext, std::shared_ptr<GBuffer> TargetBuffer,
						   std::shared_ptr<RenderCore::RHIViewPort> ViewPort, std::shared_ptr<CameraComponent> Camera,
						   std::shared_ptr<RenderCore::RHITexture2D> AntiAliasingColor);
		void BuildTonemappingPass(PostProcessGraph& Graph, RenderCore::RHICommandContext& RHIContext, std::shared_ptr<GBuffer> TargetBuffer,
								  std::shared_ptr<RenderCore::RHIViewPort> ViewPort);
	private:
		PostProcessorPrivate* d_ptr = nullptr;
	};
}