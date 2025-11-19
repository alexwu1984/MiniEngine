#pragma once
#include "Render/SimplePostProcessor.h"
#include "ShaderCommon.h"
#include "math/matrix4x4.h"

namespace Engine
{
	class BlurPS;
}

BEGIN_SHADER_STRUCT(LuquidClassContant, 0)
DECLARE_PARAM_VALUE(float, u_a, 2.4289)
DECLARE_PARAM_VALUE(float, u_b, 4.315)
DECLARE_PARAM_VALUE(float, u_c, 1.774)
DECLARE_PARAM_VALUE(float, u_d, 6.900)
DECLARE_PARAM_VALUE(float, u_fPower, 1.2330)
DECLARE_PARAM_VALUE(float, u_noise, 0.0599)
DECLARE_PARAM_VALUE(float, u_glowWeight, -0.47099)
DECLARE_PARAM_VALUE(float, u_glowBias, 0)
DECLARE_PARAM_VALUE(float, u_glowEdge0, 0.5)
DECLARE_PARAM_VALUE(float, u_glowEdge1, -0.619)
DECLARE_PARAM_VALUE(math::Vector2, rectHalfSize,math::Vector2(1,1))
DECLARE_PARAM(math::Vector3, u_midPoint)
DECLARE_PARAM_VALUE(float, pad0, 0)
DECLARE_PARAM(math::Vector2, u_quadNDC2ScreenNDCScale)
DECLARE_PARAM(math::Vector2, pad1)
BEGIN_STRUCT_CONSTRUCT(LuquidClassContant)
END_STRUCT_CONSTRUCT
END_SHADER_STRUCT

class SceneCamera
{
public:
	SceneCamera() = default;
	~SceneCamera() = default;
public:
	void SetOrthographic(float size, float nearClip, float farClip);
	void SetViewportSize(uint32_t width, uint32_t height);
	math::Matrix4x4 GetProjection() const { return m_Projection; }
protected:
	void RecalculateProjection();
private:

	float m_OrthographicSize = 10.0f;
	float m_OrthographicNearClip = -1.0f, m_OrthographicFarClip = 1.0f;
	float m_AspectRatio = 0.0f;
	math::Matrix4x4 m_Projection;
};

class LiquidClassDemo : public Engine::SimplePostProcessor
{
public:
	LiquidClassDemo(RenderCore::DynamicRHI* RHI);
	virtual ~LiquidClassDemo();

	void InitResource();
	void Draw(RenderCore::RHICommandContext& RHIContext, std::shared_ptr<RenderCore::RHIViewPort> ViewPort, float DeltaTime) override;
private:
	core::vec2f GetWorldPos(const core::vec2f& PicPos,const core::vec2f& ScreenSize);
	void ShowTexture2D(RenderCore::RHICommandContext& RHIContext, std::shared_ptr<RenderCore::RHITexture2D> Tex2D);
	void LuquidClass(RenderCore::RHICommandContext& RHIContext, std::shared_ptr<RenderCore::RHITexture2D> Tex2D);
private:
	float m_Exposure = 1.f;
	int m_MipLevel = 0;
	std::shared_ptr<RenderCore::RHIVertexShader>  m_ShowTexture2DVS;
	std::shared_ptr<RenderCore::RHIPixelShader>  m_ShowTexture2DPS;
	std::shared_ptr<RenderCore::RHIVertexShader>  m_LinquidClassVS;
	std::shared_ptr<RenderCore::RHIPixelShader>  m_LinquidClassPS;

	std::shared_ptr<RenderCore::RHIViewPort> m_ViewPort;
	std::shared_ptr<RenderCore::RHITexture2D> m_BackgroundTex;
	std::shared_ptr<RenderCore::RHITexture2D> m_LuquidSrcTex;
	std::shared_ptr<RenderCore::RHIRenderTarget> m_LuquidClassRT;
	std::shared_ptr<Engine::BlurPS> m_BlurPs;
	RenderCore::DynamicRHI* m_RHI = nullptr;
	DECLARE_SHADER_STRUCT_MEMBER(PSRenderDemoContant);
	DECLARE_SHADER_STRUCT_MEMBER(LuquidClassContant);

	SceneCamera m_Camera;
	core::vec2f m_WorldPos;
};