#pragma once
#include "core/inc.h"
#include "Render/Bloom.h"
#include "Render/PostProcessFullscreenShaders.h"
#include "Render/PostProcessGraph.h"

namespace RenderCore
{
	class DynamicRHI;
	class FXAA;
	class RHICommandContext;
	class RHIPixelShader;
	class RHITexture2D;
	class RHIVertexShader;
	class RHIViewPort;
}

namespace Engine
{
	class Bloom;
	class CameraComponent;
	class GBuffer;
	class SSRProcessor;
	class TemporallAA;

	class TonemappingPass
	{
	public:
		TonemappingPass(RenderCore::DynamicRHI* RHI, std::shared_ptr<RenderCore::RHIVertexShader> VertexShader);

		void InitResource();
		RenderPassDesc BuildDesc(RenderCore::RHICommandContext& RHIContext, std::shared_ptr<GBuffer> TargetBuffer,
								 std::shared_ptr<RenderCore::RHIViewPort> ViewPort,
								 std::function<std::shared_ptr<RenderCore::RHITexture2D>()> SourceTexture) const;

	private:
		void Execute(RenderCore::RHICommandContext& RHIContext, std::shared_ptr<GBuffer> TargetBuffer,
					 std::shared_ptr<RenderCore::RHIViewPort> ViewPort,
					 std::function<std::shared_ptr<RenderCore::RHITexture2D>()> SourceTexture) const;

		RenderCore::DynamicRHI* RHI = nullptr;
		std::shared_ptr<RenderCore::RHIVertexShader> VertexShader;
		std::shared_ptr<RenderCore::RHIPixelShader> PixelShader;
	};

	class SSRPass
	{
	public:
		SSRPass(RenderCore::RHICommandContext& RHIContext, std::shared_ptr<GBuffer> TargetBuffer,
				std::shared_ptr<RenderCore::RHIViewPort> ViewPort, std::shared_ptr<CameraComponent> Camera,
				std::shared_ptr<SSRProcessor> SSR,
				std::function<std::shared_ptr<RenderCore::RHITexture2D>()> ReflectionColor);

		RenderPassDesc BuildDesc() const;

	private:
		void Execute() const;

		RenderCore::RHICommandContext& RHIContext;
		std::shared_ptr<GBuffer> TargetBuffer;
		std::shared_ptr<RenderCore::RHIViewPort> ViewPort;
		std::shared_ptr<CameraComponent> Camera;
		std::shared_ptr<SSRProcessor> SSR;
		std::function<std::shared_ptr<RenderCore::RHITexture2D>()> ReflectionColor;
	};

	class BloomPass
	{
	public:
		BloomPass(RenderCore::RHICommandContext& RHIContext, std::shared_ptr<GBuffer> TargetBuffer,
				  std::shared_ptr<RenderCore::RHIViewPort> ViewPort, std::shared_ptr<Bloom> BloomEffect,
				  std::function<std::shared_ptr<RenderCore::RHITexture2D>()> SourceTexture);

		RenderPassDesc BuildDesc() const;

	private:
		void Execute() const;

		RenderCore::RHICommandContext& RHIContext;
		std::shared_ptr<GBuffer> TargetBuffer;
		std::shared_ptr<RenderCore::RHIViewPort> ViewPort;
		std::shared_ptr<Bloom> BloomEffect;
		std::function<std::shared_ptr<RenderCore::RHITexture2D>()> SourceTexture;
	};

	class ApplyBloomPass
	{
	public:
		ApplyBloomPass(RenderCore::DynamicRHI* RHI, std::shared_ptr<RenderCore::RHIVertexShader> VertexShader,
					   BloomContantsWrap* BloomConstants);

		void InitResource();
		RenderPassDesc BuildDesc(RenderCore::RHICommandContext& RHIContext, std::shared_ptr<GBuffer> TargetBuffer,
								 std::shared_ptr<RenderCore::RHIViewPort> ViewPort,
								 std::function<std::shared_ptr<RenderCore::RHITexture2D>()> SourceTexture,
								 std::function<std::shared_ptr<RenderCore::RHITexture2D>()> BloomTexture) const;

	private:
		void Execute(RenderCore::RHICommandContext& RHIContext, std::shared_ptr<GBuffer> TargetBuffer,
					 std::shared_ptr<RenderCore::RHIViewPort> ViewPort,
					 std::function<std::shared_ptr<RenderCore::RHITexture2D>()> SourceTexture,
					 std::function<std::shared_ptr<RenderCore::RHITexture2D>()> BloomTexture) const;

		RenderCore::DynamicRHI* RHI = nullptr;
		std::shared_ptr<RenderCore::RHIVertexShader> VertexShader;
		std::shared_ptr<RenderCore::RHIPixelShader> PixelShader;
		BloomContantsWrap* BloomConstants = nullptr;
	};

	class ApplySSRPass
	{
	public:
		ApplySSRPass(RenderCore::DynamicRHI* RHI, std::shared_ptr<RenderCore::RHIVertexShader> VertexShader);

		void InitResource();
		RenderPassDesc BuildDesc(RenderCore::RHICommandContext& RHIContext, std::shared_ptr<GBuffer> TargetBuffer,
								 std::shared_ptr<RenderCore::RHIViewPort> ViewPort,
								 std::function<std::shared_ptr<RenderCore::RHITexture2D>()> SSRTexture) const;

	private:
		void Execute(RenderCore::RHICommandContext& RHIContext, std::shared_ptr<GBuffer> TargetBuffer,
					 std::shared_ptr<RenderCore::RHIViewPort> ViewPort,
					 std::function<std::shared_ptr<RenderCore::RHITexture2D>()> SSRTexture) const;

		RenderCore::DynamicRHI* RHI = nullptr;
		std::shared_ptr<RenderCore::RHIVertexShader> VertexShader;
		std::shared_ptr<RenderCore::RHIPixelShader> PixelShader;
	};

	class TAAPass
	{
	public:
		TAAPass(RenderCore::RHICommandContext& RHIContext, std::shared_ptr<GBuffer> TargetBuffer,
				std::shared_ptr<RenderCore::RHIViewPort> ViewPort, std::shared_ptr<CameraComponent> Camera,
				std::shared_ptr<TemporallAA> TAA,
				std::function<std::shared_ptr<RenderCore::RHITexture2D>()> SourceTexture);

		RenderPassDesc BuildDesc() const;

	private:
		void Execute() const;

		RenderCore::RHICommandContext& RHIContext;
		std::shared_ptr<GBuffer> TargetBuffer;
		std::shared_ptr<RenderCore::RHIViewPort> ViewPort;
		std::shared_ptr<CameraComponent> Camera;
		std::shared_ptr<TemporallAA> TAA;
		std::function<std::shared_ptr<RenderCore::RHITexture2D>()> SourceTexture;
	};

	class FXAAPass
	{
	public:
		FXAAPass(RenderCore::RHICommandContext& RHIContext, std::shared_ptr<RenderCore::RHIViewPort> ViewPort,
				 std::shared_ptr<RenderCore::FXAA> FXAA,
				 std::function<std::shared_ptr<RenderCore::RHITexture2D>()> SourceTexture);

		RenderPassDesc BuildDesc() const;

	private:
		void Execute() const;

		RenderCore::RHICommandContext& RHIContext;
		std::shared_ptr<RenderCore::RHIViewPort> ViewPort;
		std::shared_ptr<RenderCore::FXAA> FXAA;
		std::function<std::shared_ptr<RenderCore::RHITexture2D>()> SourceTexture;
	};
}
