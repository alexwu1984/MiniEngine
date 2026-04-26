#pragma once

#include "DemoRunner/Demos/IDemo.h"

#include "ShaderCommon.h"

#include <memory>
#include <string>
#include <vector>

namespace Engine
{
	class IBLRender;
	class CubeMapCrossRender;
}

namespace RenderCore
{
	class RHIVertexShader;
	class RHIPixelShader;
	class RHITexture2D;
	class RHITextureCube;
}

namespace DemoRunner
{
	class IBLRenderDemoRunner final : public IDemo
	{
	public:
		explicit IBLRenderDemoRunner(RenderCore::DynamicRHI* RHI);
		~IBLRenderDemoRunner() override;

		const char* GetName() const override { return "ibl"; }
		ClearColor GetClearColor() const override { return { Clear.x, Clear.y, Clear.z, 1.0f }; }

		void Init(RenderCore::DynamicRHI* RHI,
				  const std::shared_ptr<RenderCore::RHIViewPort>& ViewPort,
				  const std::shared_ptr<Engine::AppWindow>& Window) override;

		void OnGui() override;

		void Draw(RenderCore::RHICommandContext& Ctx,
				  const std::shared_ptr<RenderCore::RHIViewPort>& ViewPort,
				  float DeltaTime) override;

	private:
		void GenerateIBLMaps();
		void ShowTexture2D(RenderCore::RHICommandContext& Ctx, const std::shared_ptr<RenderCore::RHITexture2D>& Texture2D);
		void ShowSHCubeMapDebugView(RenderCore::RHICommandContext& Ctx, const std::shared_ptr<RenderCore::RHITextureCube>& Cube);

	private:
		RenderCore::DynamicRHI* RHI = nullptr;
		std::shared_ptr<RenderCore::RHIViewPort> ViewPort;
		std::shared_ptr<Engine::AppWindow> Window;

		std::shared_ptr<Engine::IBLRender> IBL;
		std::shared_ptr<Engine::CubeMapCrossRender> CubeCross;

		std::vector<std::string> AllHDRFiles;
		int ChooseHDR = 0;
		int CurrentHDR = -1;

		enum ShowMode
		{
			SM_LongLat,
			SM_CubeCross,
			SM_Irradiance,
			SM_Prefiltered,
			SM_PreintegratedGF,
		};
		int Mode = SM_LongLat;
		math::Vector3 Clear = math::Vector3(0.2f);
		float Exposure = 1.0f;
		int MipLevel = 0;

		std::shared_ptr<RenderCore::RHIVertexShader> ShowTexture2DVS;
		std::shared_ptr<RenderCore::RHIPixelShader> ShowTexture2DPS;
		std::shared_ptr<RenderCore::RHIVertexShader> CubeCrossVS;
		std::shared_ptr<RenderCore::RHIPixelShader> CubeCrossPS;

		DECLARE_SHADER_STRUCT_MEMBER(PSRenderDemoContant);
		Engine::DECLARE_SHADER_STRUCT_MEMBER(CBPerFrame);
		Engine::DECLARE_SHADER_STRUCT_MEMBER(CBPerObject);
	};
}

