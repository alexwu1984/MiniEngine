#pragma once
#include "Render/SimplePostProcessor.h"
#include "ShaderCommon.h"

class LuquidClassDemo : public Engine::SimplePostProcessor
{
public:
	LuquidClassDemo(RenderCore::DynamicRHI* RHI);
	virtual ~LuquidClassDemo();

	void InitResource();
	void Draw(RenderCore::RHICommandContext& RHIContext, std::shared_ptr<RenderCore::RHIViewPort> ViewPort, float DeltaTime) override;
private:
	void ShowTexture2D(RenderCore::RHICommandContext& RHIContext, std::shared_ptr<RenderCore::RHITexture2D> Tex2D);
private:
	float m_Exposure = 1.f;
	int m_MipLevel = 0;
	std::shared_ptr<RenderCore::RHIVertexShader>  m_ShowTexture2DVS;
	std::shared_ptr<RenderCore::RHIPixelShader>  m_ShowTexture2DPS;
	std::shared_ptr<RenderCore::RHIViewPort> m_ViewPort;
	std::shared_ptr<RenderCore::RHITexture2D> m_BackgroundTex;
	RenderCore::DynamicRHI* m_RHI = nullptr;
	DECLARE_SHADER_STRUCT_MEMBER(PSRenderDemoContant);
};