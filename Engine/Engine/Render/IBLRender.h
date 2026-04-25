#pragma once
#include "core/inc.h"
#include "tinygltf/json.h"

namespace RenderCore
{
	class RHICommandContext;
	class DynamicRHI;
	class RHITextureCube;
	class RHITexture2D;
}

namespace Engine
{
	struct IBLRenderPrivate;

	class IBLRender
	{
	public:
		IBLRender(RenderCore::DynamicRHI* RHI);
		~IBLRender();

		void InitResource();
		void LoadConfig(const nlohmann::json& Root);
		void LoadTex(const std::wstring& FileName);
		void Draw(RenderCore::RHICommandContext& RHIContext);
		std::shared_ptr<RenderCore::RHITextureCube> GetPreFilterCube();
		std::shared_ptr<RenderCore::RHITextureCube> GetIrrCube();
		std::shared_ptr<RenderCore::RHITextureCube> GetEvnCube();
		std::shared_ptr<RenderCore::RHITexture2D> GetPreIntegrateBRDF();
		std::shared_ptr<RenderCore::RHITexture2D> GetHDRTex();
	private:
		void GenerateCubeMap(RenderCore::RHICommandContext& RHIContext);
		void GenerateIrradianceMap(RenderCore::RHICommandContext& RHIContext);
		void GeneratePrefilteredMap(RenderCore::RHICommandContext& RHIContext);
		void PreIntegrateBRDF();
	private:
		void InitShader();
		void RenderCube(RenderCore::RHICommandContext& RHIContext);
	private:
		IBLRenderPrivate* d_ptr = nullptr;
	};
}