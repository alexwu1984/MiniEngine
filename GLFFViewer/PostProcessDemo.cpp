#include "PostProcessDemo.h"
#include "core/system.h"
#include "RHI/RHIShdader.h"
#include "RHI/RHIShaderDefine.h"
#include "RHI/RHIPipeLineState.h"
#include "RHI/RHICachedStates.h"
#include "RHI/DynamicRHI.h"
#include "RHI/RHIViewPort.h"
#include "Engine/Engine.h"
#include "Render/GBuffer.h"
#include "Render/TemporalAA.h"
#include "Render/Bloom.h"
#include "Render/RenderUtil.h"

using namespace Engine;

namespace RenderCore
{
	extern std::shared_ptr<uint8_t> GetImageData(const std::wstring& path, int32_t& SizeX, int32_t& SizeY);
}


PostProcessorDemo::PostProcessorDemo(RenderCore::DynamicRHI* RHI)
	:_RHI(RHI), GET_SHADER_STRUCT_MEMBER(cbTransition1)(RHI)
{

}

PostProcessorDemo::~PostProcessorDemo()
{
}

void PostProcessorDemo::InitResource()
{
	std::wstring ShaderPath = core::process_directory().wstring() + L"/ShaderLibDX/";
	ShaderPath += L"PostProcessDemo.hlsl";

	_VertexShader = _RHI->RHICreateVertexShader(ShaderPath, "VS_ScreenQuad", {}, {});
	_PixelShader = _RHI->RHICreatePixelShader(ShaderPath, "PS_Transition1", {});

	std::wstring TexPath = core::process_directory().wstring() + L"/GLTFModel/";
	_Texture1 = _RHI->RHICreateTexture2D(TexPath + L"tifa_wallpaper_3840x2160.jpg");
	_Texture2 = _RHI->RHICreateTexture2D(TexPath + L"aerith_wallpaper_3840x2160.jpg");

}

void PostProcessorDemo::Draw(RenderCore::RHICommandContext& RHIContext, std::shared_ptr<RenderCore::RHIViewPort> ViewPort, float DeltaTime)
{
	if (!_Texture1 || !_Texture2)
	{
		return;
	}

	RenderCore::GraphicsPipelineStateInitializer Init;
	Init.VertexShader = _VertexShader;
	Init.PixelShader = _PixelShader;

	Init.BlendState = RenderCore::RHICachedStates::BlendTraditional;
	Init.DepthStencilState = RenderCore::RHICachedStates::DepthStateDisable;
	Init.RasterizerState = RenderCore::RHICachedStates::RasterizerStateCullNone;

	RHIContext.RHISetGraphicsPipelineState(Init);
	RHIContext.RHISetShaderSampler(RenderCore::SF_Pixel, 0, RenderCore::RHICachedStates::WarpLinerSampler);
	RHIContext.RHISetShaderTexture(RenderCore::SF_Pixel, 0, _Texture1);
	RHIContext.RHISetShaderTexture(RenderCore::SF_Pixel, 1, _Texture2);
	GET_UNIFORMDATA(cbTransition1).progress += DeltaTime*0.1f;
	if (GET_UNIFORMDATA(cbTransition1).progress >=1.0f)
	{
		GET_UNIFORMDATA(cbTransition1).progress = 0.f;
	}
	GET_SHADER_STRUCT_MEMBER(cbTransition1).UpdateUniformBuffer();
	GET_SHADER_STRUCT_MEMBER(cbTransition1).SetShaderUniformBuffer(RenderCore::EShaderFrequency::SF_Pixel);
	RHIContext.Draw(3);
}
