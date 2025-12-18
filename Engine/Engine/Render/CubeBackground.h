#pragma once
#include "core/inc.h"

namespace RenderCore
{
	class RHICommandContext;
	class DynamicRHI;
	class RHITextureCube;
	class RHITexture2D;
}


namespace Engine
{
	struct CubeBackgroundPrivate;

	class CubeBackground
	{
	public:
		CubeBackground(RenderCore::DynamicRHI* RHI);
		~CubeBackground();

		void InitResource();
		void Render(RenderCore::RHICommandContext& RHIContext, 
					const std::vector <std::shared_ptr<RenderCore::RHITexture2D>>& Targets, 
					std::shared_ptr<RenderCore::RHITexture2D> Depth);
		void SetTextureCube(std::shared_ptr<RenderCore::RHITextureCube> TexCube);
		void SetRotate(float xRotate, float yRotate);
	private:
		void InitShader();
	private:
		CubeBackgroundPrivate* d_ptr = nullptr;
	};
}