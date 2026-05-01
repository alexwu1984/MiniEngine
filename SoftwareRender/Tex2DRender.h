#pragma once
#include "core/inc.h"
#include "RHI/RHIShdader.h"
#include "RHI/RHITexture2D.h"
#include "RHI/RHIShaderDefine.h"
#include "Render/MaterialPreFrame.h"

/** Same layout as GLFFViewer/ShaderCommon.h PSRenderDemoContant (register b5). */
struct PSRenderDemoContant
{
	float Exposure{};
	int32_t MipLevel{};
	int32_t MaxMipLevel{};
	int32_t NumSamplesPerDir{};
};
using PSRenderDemoContantWrap = RenderCore::TUniformBufferBinding<PSRenderDemoContant, 5u>;

class Tex2DRender
{
public:
	Tex2DRender(RenderCore::DynamicRHI* RHI);
	virtual ~Tex2DRender();

	void InitResource();
	void InitTex(const core::vec2u& texSize, const uint8_t* buffer);
	/** hostClientWidth/Height: swapchain client area for letterboxed blit (e.g. from AppWindow). */
	void Draw(RenderCore::RHICommandContext& RHIContext, std::shared_ptr<RenderCore::RHIViewPort> ViewPort, float DeltaTime,
			   int32_t hostClientWidth, int32_t hostClientHeight);
private:
	void ShowTexture2D(RenderCore::RHICommandContext& RHIContext, std::shared_ptr<RenderCore::RHITexture2D> Tex2D,
					   std::shared_ptr<RenderCore::RHIViewPort> ViewPort, int32_t hostClientWidth, int32_t hostClientHeight);
private:
	std::shared_ptr<RenderCore::RHITexture2D> m_tex2D;
	math::Vector3 m_ClearColor = math::Vector3(0.2f);
	float m_Exposure = 1.f;
	int m_MipLevel = 0;
	std::shared_ptr<RenderCore::RHIVertexShader>  m_ShowTexture2DVS;
	std::shared_ptr<RenderCore::RHIPixelShader>  m_ShowTexture2DPS;
	RenderCore::DynamicRHI* m_RHI = nullptr;
	DECLARE_SHADER_STRUCT_MEMBER(PSRenderDemoContant);
	Engine::DECLARE_SHADER_STRUCT_MEMBER(CBPerFrame);
	Engine::DECLARE_SHADER_STRUCT_MEMBER(CBPerObject);
};
