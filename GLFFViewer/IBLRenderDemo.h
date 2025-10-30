#pragma once
#include "Render/SimplePostProcessor.h"
#include "ShaderCommon.h"

namespace Engine
{
	class IBLRender;
	class CubeMapCrossRender;
}

enum EShowMode
{
	SM_LongLat,
	SM_CubeMapCross,
	SM_Irradiance,
	SM_Prefiltered,
	SM_SphericalHarmonics,
	SM_PreintegratedGF,
	//SM_PBR,
};

class IBLRenderDemo : public Engine::SimplePostProcessor
{
public:
	IBLRenderDemo(RenderCore::DynamicRHI* RHI);
	virtual ~IBLRenderDemo();

	void InitResource();
	void Draw(RenderCore::RHICommandContext& RHIContext, std::shared_ptr<RenderCore::RHIViewPort> ViewPort, float DeltaTime) override;
private:
	void GenerateIBLMaps();
	void ShowTexture2D(RenderCore::RHICommandContext& RHIContext, std::shared_ptr<RenderCore::RHITexture2D> Tex2D);
	void ShowSHCubeMapDebugView(RenderCore::RHICommandContext& RHIContext, std::shared_ptr<RenderCore::RHITextureCube> Cube);
private:
	std::shared_ptr<Engine::IBLRender> m_IBLRender;
	std::shared_ptr<Engine::CubeMapCrossRender> m_CubeMapCrossRender;
	std::vector<std::string> m_AllHDRFiles;
	int m_ChooseHDR = 0;
	int m_CurrentHDR = -1;
	int m_ShowMode = SM_LongLat;
	math::Vector3 m_ClearColor = math::Vector3(0.2f);
	float m_Exposure = 1.f;
	int m_MipLevel = 0;
	std::shared_ptr<RenderCore::RHIVertexShader>  m_ShowTexture2DVS;
	std::shared_ptr<RenderCore::RHIPixelShader>  m_ShowTexture2DPS;
	std::shared_ptr<RenderCore::RHIPixelShader> m_GenIrradiancePS;
	std::shared_ptr<RenderCore::RHIPixelShader> m_GenPrefilterPS;
	std::shared_ptr<RenderCore::RHIVertexShader> m_CubeMapCrossVS;
	std::shared_ptr<RenderCore::RHIPixelShader>  m_CubeMapCrossPS;
	std::shared_ptr<RenderCore::RHIViewPort> m_ViewPort;
	RenderCore::DynamicRHI* m_RHI = nullptr;
	DECLARE_SHADER_STRUCT_MEMBER(PSRenderDemoContant);
	Engine::DECLARE_SHADER_STRUCT_MEMBER(CBPerFrame);
	Engine::DECLARE_SHADER_STRUCT_MEMBER(CBPerObject);
};