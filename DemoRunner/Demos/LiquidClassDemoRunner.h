#pragma once

#include "DemoRunner/Demos/IDemo.h"

#include "ShaderCommon.h"
#include "math/matrix4x4.h"

#include <memory>

namespace Engine { class BlurPS; }
namespace RenderCore
{
	class RHIVertexShader;
	class RHIPixelShader;
	class RHITexture2D;
	class RHIRenderTarget;
}

namespace DemoRunner
{
	struct LuquidClassContant
	{
		float u_a{ 2.4289f };
		float u_b{ 4.315f };
		float u_c{ 1.774f };
		float u_d{ 6.900f };
		float u_fPower{ 3.2330f };
		float u_noise{ 0.0599f };
		float u_glowWeight{ -0.47099f };
		float u_glowBias{ 0.0f };
		float u_glowEdge0{ 0.5f };
		float u_glowEdge1{ -0.619f };
		math::Vector2 rectHalfSize{ 1.f, 1.f };
		math::Vector3 u_midPoint{};
		float pad0{ 0.f };
		math::Vector2 u_quadNDC2ScreenNDCScale{};
		math::Vector2 pad1{};
	};
	using LuquidClassContantWrap = RenderCore::TUniformBufferBinding<LuquidClassContant, 0u>;

	class SceneCamera
	{
	public:
		void SetOrthographic(float size, float nearClip, float farClip);
		void SetViewportSize(uint32_t width, uint32_t height);
		math::Matrix4x4 GetProjection() const { return Projection; }

	private:
		void RecalculateProjection();
		float OrthoSize = 10.0f;
		float OrthoNear = -1.0f, OrthoFar = 1.0f;
		float Aspect = 0.0f;
		math::Matrix4x4 Projection;
	};

	class LiquidClassDemoRunner final : public IDemo
	{
	public:
		explicit LiquidClassDemoRunner(RenderCore::DynamicRHI* RHI);
		~LiquidClassDemoRunner() override;

		const char* GetName() const override { return "liquid"; }

		void Init(RenderCore::DynamicRHI* RHI,
				  const std::shared_ptr<RenderCore::RHIViewPort>& ViewPort,
				  const std::shared_ptr<Engine::AppWindow>& Window) override;

		void OnGui() override;

		void Draw(RenderCore::RHICommandContext& Ctx,
				  const std::shared_ptr<RenderCore::RHIViewPort>& ViewPort,
				  float DeltaTime) override;

	private:
		core::vec2f GetWorldPos(const core::vec2f& PicPos, const core::vec2f& ScreenSize);
		void ShowTexture2D(RenderCore::RHICommandContext& Ctx, const std::shared_ptr<RenderCore::RHITexture2D>& Tex2D);
		void LiquidClass(RenderCore::RHICommandContext& Ctx, const std::shared_ptr<RenderCore::RHITexture2D>& Tex2D);

	private:
		RenderCore::DynamicRHI* RHI = nullptr;
		std::shared_ptr<RenderCore::RHIViewPort> ViewPort;
		std::shared_ptr<Engine::AppWindow> Window;

		float Exposure = 1.0f;
		std::shared_ptr<RenderCore::RHIVertexShader> ShowTexture2DVS;
		std::shared_ptr<RenderCore::RHIPixelShader> ShowTexture2DPS;
		std::shared_ptr<RenderCore::RHIVertexShader> LiquidVS;
		std::shared_ptr<RenderCore::RHIPixelShader> LiquidPS;

		std::shared_ptr<RenderCore::RHITexture2D> BackgroundTex;
		std::shared_ptr<RenderCore::RHITexture2D> LiquidSrcTex;
		std::shared_ptr<RenderCore::RHIRenderTarget> LiquidRT;
		std::shared_ptr<Engine::BlurPS> Blur;

		SceneCamera Camera;

		DECLARE_SHADER_STRUCT_MEMBER(PSRenderDemoContant);
		DECLARE_SHADER_STRUCT_MEMBER(LuquidClassContant);
	};
}

