#pragma once
#include "Render/SimplePostProcessor.h"
#include "RHI/RHIShdader.h"
#include "RHI/RHITexture2D.h"
#include "RHI/RHIShaderDefine.h"
#include "Render/MaterialPreFrame.h"


BEGIN_SHADER_STRUCT(PSRenderDemoContant, 5)
	DECLARE_PARAM(float, Exposure)
	DECLARE_PARAM(int32_t, MipLevel)
	DECLARE_PARAM(int32_t, MaxMipLevel)
	DECLARE_PARAM(int32_t, NumSamplesPerDir)
BEGIN_STRUCT_CONSTRUCT(PSRenderDemoContant)
END_STRUCT_CONSTRUCT
END_SHADER_STRUCT

class Tex2DRender : public Engine::SimplePostProcessor
{
public:
	Tex2DRender(RenderCore::DynamicRHI* RHI);
	virtual ~Tex2DRender();

	void InitResource();
	void InitTex(const core::vec2u& texSize, const uint8_t* buffer);
	void Draw(RenderCore::RHICommandContext& RHIContext, std::shared_ptr<RenderCore::RHIViewPort> ViewPort, float DeltaTime) override;
private:
	void ShowTexture2D(RenderCore::RHICommandContext& RHIContext, std::shared_ptr<RenderCore::RHITexture2D> Tex2D);
private:
	std::shared_ptr<RenderCore::RHITexture2D> m_tex2D;
	math::Vector3 m_ClearColor = math::Vector3(0.2f);
	float m_Exposure = 1.f;
	int m_MipLevel = 0;
	std::shared_ptr<RenderCore::RHIVertexShader>  m_ShowTexture2DVS;
	std::shared_ptr<RenderCore::RHIPixelShader>  m_ShowTexture2DPS;
	std::shared_ptr<RenderCore::RHIViewPort> m_ViewPort;
	RenderCore::DynamicRHI* m_RHI = nullptr;
	DECLARE_SHADER_STRUCT_MEMBER(PSRenderDemoContant);
	Engine::DECLARE_SHADER_STRUCT_MEMBER(CBPerFrame);
	Engine::DECLARE_SHADER_STRUCT_MEMBER(CBPerObject);
};